package main

import (
	"bufio"
	"bytes"
	"crypto/rand"
	"encoding/hex"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"sync"
	"time"
)

const (
	completionEnvironment = "FIXTURE_COMPLETION_TOKEN"
	completionPrefix      = "fixture-complete:"
	maximumOutputBytes    = 1 << 20
)

func main() {
	tokenBytes := make([]byte, 24)
	if _, err := rand.Read(tokenBytes); err != nil {
		fmt.Fprintln(os.Stderr, "create completion token:", err)
		os.Exit(1)
	}
	token := hex.EncodeToString(tokenBytes)
	validatorBinary := filepath.Join(filepath.Dir(os.Args[0]), "validator-tests")
	candidateBinary := filepath.Join(filepath.Dir(os.Args[0]), "candidate-tests")

	command := exec.Command(validatorBinary, "-test.timeout=8s")
	command.Env = append(os.Environ(), completionEnvironment+"="+token)
	var output limitedOutput
	command.Stdout = &output
	command.Stderr = &output

	done := make(chan error, 1)
	go func() {
		done <- command.Run()
	}()

	var runError error
	select {
	case runError = <-done:
	case <-time.After(10 * time.Second):
		if command.Process != nil {
			_ = command.Process.Kill()
		}
		<-done
		runError = fmt.Errorf("candidate tests exceeded supervisor timeout")
	}
	outputBytes, truncated := output.snapshot()
	_, _ = os.Stdout.Write(outputBytes)
	if truncated {
		fmt.Fprintln(os.Stderr, "candidate test output exceeded 1 MiB")
		os.Exit(1)
	}
	if runError != nil {
		fmt.Fprintln(os.Stderr, runError)
		os.Exit(1)
	}

	expected := completionPrefix + token
	found := false
	scanner := bufio.NewScanner(bytes.NewReader(outputBytes))
	for scanner.Scan() {
		if strings.TrimSpace(scanner.Text()) == expected {
			found = true
			break
		}
	}
	if !found {
		fmt.Fprintln(os.Stderr, "validator tests exited without their completion token")
		os.Exit(1)
	}

	candidate := exec.Command(candidateBinary, "-test.timeout=3s")
	candidate.Stdout = &output
	candidate.Stderr = &output
	candidateDone := make(chan error, 1)
	go func() {
		candidateDone <- candidate.Run()
	}()
	select {
	case runError = <-candidateDone:
	case <-time.After(5 * time.Second):
		if candidate.Process != nil {
			_ = candidate.Process.Kill()
		}
		<-candidateDone
		runError = fmt.Errorf("candidate-authored tests exceeded supervisor timeout")
	}
	updatedOutput, updatedTruncated := output.snapshot()
	if len(updatedOutput) > len(outputBytes) {
		_, _ = os.Stdout.Write(updatedOutput[len(outputBytes):])
	}
	if updatedTruncated {
		fmt.Fprintln(os.Stderr, "combined test output exceeded 1 MiB")
		os.Exit(1)
	}
	if runError != nil {
		fmt.Fprintln(os.Stderr, runError)
		os.Exit(1)
	}
}

type limitedOutput struct {
	mu        sync.Mutex
	buffer    bytes.Buffer
	truncated bool
}

func (output *limitedOutput) Write(data []byte) (int, error) {
	output.mu.Lock()
	defer output.mu.Unlock()

	remaining := maximumOutputBytes - output.buffer.Len()
	if remaining > 0 {
		length := len(data)
		if length > remaining {
			length = remaining
		}
		_, _ = output.buffer.Write(data[:length])
	}
	if len(data) > remaining {
		output.truncated = true
	}
	return len(data), nil
}

func (output *limitedOutput) snapshot() ([]byte, bool) {
	output.mu.Lock()
	defer output.mu.Unlock()
	return bytes.Clone(output.buffer.Bytes()), output.truncated
}

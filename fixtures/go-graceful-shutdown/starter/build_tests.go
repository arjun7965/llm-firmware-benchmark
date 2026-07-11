package main

import (
	"errors"
	"flag"
	"fmt"
	"go/ast"
	"go/parser"
	"go/token"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
)

const modulePath = "fixture.local/go-graceful-shutdown"

type config struct {
	answer       string
	tests        string
	supervisor   string
	moduleRoot   string
	testBinary   string
	outputBinary string
}

func main() {
	configuration, err := parseConfig()
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(2)
	}
	if err := build(configuration); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}

func parseConfig() (config, error) {
	var configuration config
	flag.StringVar(&configuration.answer, "answer", "", "candidate Go source")
	flag.StringVar(&configuration.tests, "tests", "", "public Go tests")
	flag.StringVar(&configuration.supervisor, "supervisor", "", "test supervisor source")
	flag.StringVar(&configuration.moduleRoot, "module-root", "", "staged module directory")
	flag.StringVar(&configuration.testBinary, "test-binary", "", "candidate test output")
	flag.StringVar(&configuration.outputBinary, "output", "", "supervisor output")
	flag.Parse()

	if flag.NArg() != 0 {
		return config{}, errors.New("unexpected positional arguments")
	}
	for name, value := range map[string]string{
		"answer":      configuration.answer,
		"tests":       configuration.tests,
		"supervisor":  configuration.supervisor,
		"module-root": configuration.moduleRoot,
		"test-binary": configuration.testBinary,
		"output":      configuration.outputBinary,
	} {
		if value == "" {
			return config{}, fmt.Errorf("--%s is required", name)
		}
	}
	if filepath.Base(configuration.moduleRoot) != "module" ||
		filepath.Base(filepath.Dir(configuration.moduleRoot)) != "build" {
		return config{}, errors.New("module root must be build/module")
	}
	if filepath.Dir(configuration.testBinary) != filepath.Dir(configuration.outputBinary) ||
		filepath.Base(configuration.testBinary) != "candidate-tests" ||
		filepath.Base(configuration.outputBinary) != "public-tests" {
		return config{}, errors.New("test binaries must use their declared sibling paths")
	}
	paths := []*string{
		&configuration.answer,
		&configuration.tests,
		&configuration.supervisor,
		&configuration.moduleRoot,
		&configuration.testBinary,
		&configuration.outputBinary,
	}
	for _, path := range paths {
		absolute, err := filepath.Abs(*path)
		if err != nil {
			return config{}, fmt.Errorf("resolve %s: %w", *path, err)
		}
		*path = absolute
	}
	return configuration, nil
}

func build(configuration config) error {
	if err := validateCandidateSource(configuration.answer); err != nil {
		return fmt.Errorf("candidate source policy: %w", err)
	}
	if err := os.RemoveAll(configuration.moduleRoot); err != nil {
		return fmt.Errorf("clear staged module: %w", err)
	}
	for _, directory := range []string{
		filepath.Join(configuration.moduleRoot, "generated"),
		filepath.Join(configuration.moduleRoot, "supervisor"),
		filepath.Join(configuration.moduleRoot, "tests", "public"),
	} {
		if err := os.MkdirAll(directory, 0o700); err != nil {
			return fmt.Errorf("create staged module: %w", err)
		}
	}

	if err := os.WriteFile(
		filepath.Join(configuration.moduleRoot, "go.mod"),
		[]byte("module "+modulePath+"\n\ngo 1.24.4\n"),
		0o600,
	); err != nil {
		return fmt.Errorf("write staged go.mod: %w", err)
	}
	for source, destination := range map[string]string{
		configuration.answer: filepath.Join(
			configuration.moduleRoot,
			"generated",
			"answer.go",
		),
		configuration.tests: filepath.Join(
			configuration.moduleRoot,
			"tests",
			"public",
			"server_test.go",
		),
		configuration.supervisor: filepath.Join(
			configuration.moduleRoot,
			"supervisor",
			"main.go",
		),
	} {
		if err := copyFile(source, destination); err != nil {
			return err
		}
	}

	if err := runGo(
		configuration.moduleRoot,
		"test",
		"-buildvcs=false",
		"-trimpath",
		"-c",
		"-o",
		configuration.testBinary,
		"./tests/public",
	); err != nil {
		return fmt.Errorf("compile candidate tests: %w", err)
	}
	if err := runGo(
		configuration.moduleRoot,
		"build",
		"-buildvcs=false",
		"-trimpath",
		"-o",
		configuration.outputBinary,
		"./supervisor",
	); err != nil {
		return fmt.Errorf("compile test supervisor: %w", err)
	}
	return nil
}

func validateCandidateSource(source string) error {
	file, err := parser.ParseFile(
		token.NewFileSet(),
		source,
		nil,
		parser.SkipObjectResolution,
	)
	if err != nil {
		return err
	}
	if file.Name.Name != "shutdown" {
		return errors.New("package must be shutdown")
	}
	for _, declaration := range file.Decls {
		function, ok := declaration.(*ast.FuncDecl)
		if ok && function.Recv == nil && function.Name.Name == "init" {
			return errors.New("init functions are not allowed")
		}
	}

	aliases := make(map[string]string)
	for _, specification := range file.Imports {
		importPath, err := strconv.Unquote(specification.Path.Value)
		if err != nil {
			return fmt.Errorf("invalid import path: %w", err)
		}
		if !isTerminationPackage(importPath) {
			continue
		}
		alias := filepath.Base(importPath)
		if specification.Name != nil {
			alias = specification.Name.Name
		}
		if alias == "." {
			return fmt.Errorf("dot import of %s is not allowed", importPath)
		}
		if alias != "_" {
			aliases[alias] = importPath
		}
	}

	var violation error
	ast.Inspect(file, func(node ast.Node) bool {
		if violation != nil {
			return false
		}
		selector, ok := node.(*ast.SelectorExpr)
		if !ok {
			return true
		}
		identifier, ok := selector.X.(*ast.Ident)
		if !ok {
			return true
		}
		importPath, ok := aliases[identifier.Name]
		if ok && isTerminationFunction(importPath, selector.Sel.Name) {
			violation = fmt.Errorf(
				"%s.%s is not allowed",
				importPath,
				selector.Sel.Name,
			)
			return false
		}
		return true
	})
	return violation
}

func isTerminationPackage(importPath string) bool {
	return importPath == "log" || importPath == "os" ||
		importPath == "runtime" || importPath == "syscall"
}

func isTerminationFunction(importPath, name string) bool {
	switch importPath {
	case "log":
		return name == "Fatal" || name == "Fatalf" || name == "Fatalln"
	case "os", "syscall":
		return name == "Exit"
	case "runtime":
		return name == "Goexit"
	default:
		return false
	}
}

func copyFile(source, destination string) error {
	input, err := os.Open(source)
	if err != nil {
		return fmt.Errorf("open %s: %w", source, err)
	}
	defer input.Close()

	output, err := os.OpenFile(destination, os.O_CREATE|os.O_EXCL|os.O_WRONLY, 0o600)
	if err != nil {
		return fmt.Errorf("create %s: %w", destination, err)
	}
	if _, err := io.Copy(output, input); err != nil {
		output.Close()
		return fmt.Errorf("copy %s: %w", source, err)
	}
	if err := output.Close(); err != nil {
		return fmt.Errorf("close %s: %w", destination, err)
	}
	return nil
}

func runGo(directory string, arguments ...string) error {
	goExecutable := os.Getenv("FIXTURE_GO_EXECUTABLE")
	if goExecutable == "" {
		goExecutable = "go"
	}
	command := exec.Command(goExecutable, arguments...)
	command.Dir = directory
	command.Env = append(
		os.Environ(),
		"GOTOOLCHAIN=local",
		"GOWORK=off",
		"GOENV=off",
		"CGO_ENABLED=0",
	)
	command.Stdout = os.Stdout
	command.Stderr = os.Stderr
	return command.Run()
}

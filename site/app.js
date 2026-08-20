const dataElement = document.querySelector("#benchmark-data");
const benchmark = JSON.parse(dataElement.textContent);

const taskById = new Map(benchmark.tasks.map((task) => [task.id, task]));
const state = {
  suite: "all",
  category: "all",
  query: "",
};

function escapeHtml(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#039;");
}

function label(value) {
  return value
    .split("-")
    .map((part) => part.charAt(0).toUpperCase() + part.slice(1))
    .join(" ");
}

function configureLinks() {
  for (const link of document.querySelectorAll("[data-repository-link]")) {
    link.href = benchmark.repositoryUrl;
    link.target = "_blank";
    link.rel = "noreferrer";
  }
  for (const link of document.querySelectorAll("[data-docs-link]")) {
    link.href = benchmark.docsUrl;
    link.target = "_blank";
    link.rel = "noreferrer";
  }
  const tasksLink = document.querySelector("[data-tasks-link]");
  tasksLink.href = benchmark.tasksUrl;
  tasksLink.target = "_blank";
  tasksLink.rel = "noreferrer";

  const gradingLink = document.querySelector("[data-grading-link]");
  gradingLink.href = `${benchmark.repositoryUrl}/blob/main/docs/benchmarks/firmware-scoring.md`;
  gradingLink.target = "_blank";
  gradingLink.rel = "noreferrer";

  const readmeLink = document.querySelector("[data-readme-link]");
  readmeLink.href = `${benchmark.repositoryUrl}/blob/main/README.md#quick-start`;
  readmeLink.target = "_blank";
  readmeLink.rel = "noreferrer";
}

function renderStats() {
  for (const element of document.querySelectorAll("[data-stat]")) {
    element.textContent = benchmark.stats[element.dataset.stat];
  }
  const counts = {
    all: benchmark.tasks.length,
    firmware: benchmark.stats.firmwareTasks,
    auxiliary: benchmark.stats.auxiliaryTasks,
  };
  for (const element of document.querySelectorAll("[data-count]")) {
    element.textContent = counts[element.dataset.count];
  }
}

function configureCategoryFilter() {
  const select = document.querySelector("[data-category-filter]");
  const categories = [...new Set(benchmark.tasks.map((task) => task.category))]
    .sort((left, right) => left.localeCompare(right));
  for (const category of categories) {
    const option = document.createElement("option");
    option.value = category;
    option.textContent = label(category);
    select.append(option);
  }
}

function visibleTasks() {
  const query = state.query.trim().toLocaleLowerCase();
  return benchmark.tasks.filter((task) => {
    if (state.suite !== "all" && task.suite !== state.suite) return false;
    if (state.category !== "all" && task.category !== state.category) {
      return false;
    }
    if (query === "") return true;
    return [
      task.id,
      task.title,
      task.objective,
      task.category,
      task.validationProfile,
      task.targetProfile ?? "",
    ].some((value) => value.toLocaleLowerCase().includes(query));
  });
}

function taskCard(task) {
  return `
    <article class="task-card">
      <div class="task-card-top">
        <span class="task-suite ${escapeHtml(task.suite)}">${escapeHtml(task.suite)}</span>
        <span class="task-id">${escapeHtml(task.id)}</span>
      </div>
      <h3>${escapeHtml(task.title)}</h3>
      <p>${escapeHtml(task.objective)}</p>
      <div class="task-card-footer">
        <div class="task-card-meta">
          <span class="task-category">${escapeHtml(label(task.category))}</span>
          <span class="task-profile">${escapeHtml(task.validationProfile)}</span>
        </div>
        <button class="task-open" type="button" data-open-task="${escapeHtml(task.id)}">
          View scoring <span aria-hidden="true">→</span>
        </button>
      </div>
    </article>
  `;
}

function renderTasks() {
  const tasks = visibleTasks();
  const grid = document.querySelector("[data-task-grid]");
  grid.innerHTML = tasks.map(taskCard).join("");
  document.querySelector("[data-visible-count]").textContent = tasks.length;
  document.querySelector("[data-empty-state]").hidden = tasks.length !== 0;
  grid.hidden = tasks.length === 0;
}

function clearFilters() {
  state.suite = "all";
  state.category = "all";
  state.query = "";
  document.querySelector("[data-task-search]").value = "";
  document.querySelector("[data-category-filter]").value = "all";
  for (const button of document.querySelectorAll("[data-suite]")) {
    button.classList.toggle("is-active", button.dataset.suite === "all");
  }
  renderTasks();
}

function configureExplorer() {
  document.querySelector("[data-task-search]").addEventListener("input", (event) => {
    state.query = event.currentTarget.value;
    renderTasks();
  });
  document.querySelector("[data-category-filter]").addEventListener("change", (event) => {
    state.category = event.currentTarget.value;
    renderTasks();
  });
  for (const button of document.querySelectorAll("[data-suite]")) {
    button.addEventListener("click", () => {
      state.suite = button.dataset.suite;
      for (const peer of document.querySelectorAll("[data-suite]")) {
        peer.classList.toggle("is-active", peer === button);
      }
      renderTasks();
    });
  }
  document.querySelector("[data-clear-filters]").addEventListener("click", clearFilters);
}

const taskDialog = document.querySelector("[data-task-dialog]");

function openTask(taskId) {
  const task = taskById.get(taskId);
  if (!task) return;
  document.querySelector("[data-dialog-kicker]").textContent =
    `${task.suite} suite / ${task.id}`;
  document.querySelector("[data-dialog-title]").textContent = task.title;
  document.querySelector("[data-dialog-objective]").textContent = task.objective;

  const metadata = [
    label(task.category),
    `validation: ${task.validationProfile}`,
    task.targetProfile ? `target: ${task.targetProfile}` : null,
    task.scoringMode,
  ].filter(Boolean);
  document.querySelector("[data-dialog-meta]").innerHTML = metadata
    .map((item) => `<span>${escapeHtml(item)}</span>`)
    .join("");
  document.querySelector("[data-dialog-scoring]").innerHTML = task.scoring
    .map((criterion) => `
      <div class="score-criterion">
        <strong>${escapeHtml(criterion.dimension)}</strong>
        <p>${escapeHtml(criterion.evidence)}</p>
        <b>${criterion.points}</b>
      </div>
    `)
    .join("");
  const note = document.querySelector("[data-dialog-note]");
  note.textContent = task.note ?? "";
  note.hidden = !task.note;

  const rubric = document.querySelector("[data-dialog-rubric]");
  rubric.href = task.rubricUrl;
  rubric.target = "_blank";
  rubric.rel = "noreferrer";
  const fixture = document.querySelector("[data-dialog-fixture]");
  fixture.href = task.fixtureUrl;
  fixture.target = "_blank";
  fixture.rel = "noreferrer";
  taskDialog.showModal();
}

function configureDialog() {
  document.querySelector("[data-task-grid]").addEventListener("click", (event) => {
    const button = event.target.closest("[data-open-task]");
    if (button) openTask(button.dataset.openTask);
  });
  document.querySelector("[data-dialog-close]").addEventListener("click", () => {
    taskDialog.close();
  });
  taskDialog.addEventListener("click", (event) => {
    const bounds = taskDialog.getBoundingClientRect();
    const outside = event.clientX < bounds.left || event.clientX > bounds.right ||
      event.clientY < bounds.top || event.clientY > bounds.bottom;
    if (outside) taskDialog.close();
  });
}

function configureNavigation() {
  const header = document.querySelector("[data-header]");
  const nav = document.querySelector("[data-nav]");
  const toggle = document.querySelector("[data-nav-toggle]");
  const updateHeader = () => header.classList.toggle("is-scrolled", scrollY > 12);
  updateHeader();
  addEventListener("scroll", updateHeader, { passive: true });
  toggle.addEventListener("click", () => {
    const open = toggle.getAttribute("aria-expanded") !== "true";
    toggle.setAttribute("aria-expanded", String(open));
    nav.classList.toggle("is-open", open);
    header.classList.toggle("is-open", open);
  });
  for (const link of nav.querySelectorAll("a")) {
    link.addEventListener("click", () => {
      toggle.setAttribute("aria-expanded", "false");
      nav.classList.remove("is-open");
      header.classList.remove("is-open");
    });
  }
}

function configureCopyButtons() {
  for (const button of document.querySelectorAll("[data-copy-target]")) {
    button.addEventListener("click", async () => {
      const target = document.getElementById(button.dataset.copyTarget);
      try {
        await navigator.clipboard.writeText(target.innerText);
        button.textContent = "Copied";
        setTimeout(() => { button.textContent = "Copy"; }, 1600);
      } catch {
        button.textContent = "Select text";
      }
    });
  }
}

configureLinks();
renderStats();
configureCategoryFilter();
configureExplorer();
configureDialog();
configureNavigation();
configureCopyButtons();
renderTasks();

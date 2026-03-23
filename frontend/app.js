const appState = {
  data: null,
  mode: "replay",
  currentIndex: 0,
  playbackTime: 0,
  isPlaying: false,
  speed: 1,
  rafId: null,
  lastFrameAt: null,
  manual: null,
  sharedFile: {
    content: "",
    size: 0,
    updatedAt: "",
    error: "",
  },
};

/* Collect the dashboard controls once so rendering logic can stay focused on state updates. */
const controls = {
  playToggle: document.getElementById("play-toggle"),
  resetButton: document.getElementById("reset-playback"),
  prevButton: document.getElementById("prev-event"),
  nextButton: document.getElementById("next-event"),
  scrubber: document.getElementById("timeline-scrubber"),
  speedSelect: document.getElementById("speed-select"),
  replayMode: document.getElementById("mode-replay"),
  manualMode: document.getElementById("mode-manual"),
  manualReset: document.getElementById("manual-reset"),
  manualFileRefresh: document.getElementById("manual-file-refresh"),
  manualWriteSave: document.getElementById("manual-write-save"),
  manualWriteInput: document.getElementById("manual-write-input"),
};

const usersById = () => new Map((appState.data?.users ?? []).map((u) => [u.id, u]));
const currentEvent = () => appState.data?.events?.[appState.currentIndex] ?? null;
const formatSeconds = (ms) => `${(ms / 1000).toFixed(2)}s`;
const categoryLabel = (c) => c.replace(/_/g, " ");
const labelForUser = (id) => usersById().get(id) ? `${usersById().get(id).username} (#${id})` : `User #${id}`;

/* Replay cards and manual cards share the same user-state vocabulary. */
function determineUserState(userId, event) {
  if (!event) return "offline";
  if (event.writingUser === userId) return "writing";
  if (event.readingUsers.includes(userId)) return "reading";
  if (event.waitingUsers.includes(userId)) return "waiting";
  if (event.activeUsers.includes(userId)) return "active";
  return "offline";
}

/* Translate raw event categories into marker-friendly phase descriptions. */
function phaseFromEvent(event) {
  if (!event) return { title: "Idle", caption: "Run the simulation and load the generated dataset to begin the replay." };
  if (event.category === "system_start") return { title: "Boot Sequence", caption: "The tracker has started and the session gate is ready to admit users." };
  if (event.category === "login_requested") return { title: "Admission Pressure", caption: "Users are queueing for a slot behind the semaphore-protected gate." };
  if (event.category === "login_success") return { title: "Session Activated", caption: "A user has successfully entered the active session pool." };
  if (event.category === "read_start" || event.category === "read_finish") return { title: "Shared Reading", caption: "Multiple readers can overlap when no writer is blocking the resource." };
  if (event.category === "write_start" || event.category === "write_finish") return { title: "Exclusive Update", caption: "A single writer owns the file while all other access is held back." };
  if (event.category === "logout") return { title: "Session Release", caption: "A slot has been freed and the waiting queue can advance." };
  if (event.category === "system_finish") return { title: "Run Complete", caption: "All sessions have finished and the final statistics are ready for inspection." };
  return { title: "System Activity", caption: "The tracker is capturing synchronized state transitions across the whole run." };
}

/* Manual mode uses the same hero panel but summarizes the sandbox instead of replay data. */
function manualPhase() {
  ensureManualState();
  const writerText = appState.manual.writingUser != null ? "writer engaged" : "writer idle";
  return {
    title: "Manual Sandbox",
    caption: `${appState.manual.activeUsers.length} active, ${appState.manual.waitingUsers.length} waiting, ${appState.manual.readingUsers.length} reading, ${writerText}. Click users to demonstrate the concurrency rules live.`,
  };
}

/* Keep the scrubber, current event index, and elapsed time locked together. */
function setPlayback(index) {
  const total = appState.data?.events?.length ?? 0;
  appState.currentIndex = Math.min(Math.max(index, 0), Math.max(total - 1, 0));
  appState.playbackTime = currentEvent()?.elapsedMs ?? 0;
  render();
}

function syncIndexToPlaybackTime() {
  const events = appState.data?.events ?? [];
  let index = 0;
  while (index + 1 < events.length && events[index + 1].elapsedMs <= appState.playbackTime) index++;
  appState.currentIndex = index;
}

/* Stop animation cleanly before jumping to another event or mode. */
function stopPlayback() {
  appState.isPlaying = false;
  appState.lastFrameAt = null;
  if (appState.rafId) cancelAnimationFrame(appState.rafId);
  appState.rafId = null;
  render();
}

/* Advance through the exported event history using requestAnimationFrame for smooth playback. */
function tick(now) {
  if (!appState.isPlaying || !appState.data) return;
  if (appState.lastFrameAt == null) appState.lastFrameAt = now;
  appState.playbackTime += (now - appState.lastFrameAt) * appState.speed;
  appState.lastFrameAt = now;
  const end = appState.data.events.at(-1)?.elapsedMs ?? 0;
  if (appState.playbackTime >= end) {
    appState.playbackTime = end;
    syncIndexToPlaybackTime();
    stopPlayback();
    return;
  }
  syncIndexToPlaybackTime();
  render();
  appState.rafId = requestAnimationFrame(tick);
}

/* Replay mode can be played, paused, or restarted from the beginning once it reaches the end. */
function togglePlayback() {
  if (appState.mode !== "replay") return;
  if (!appState.data?.events?.length) return;
  if (appState.currentIndex >= appState.data.events.length - 1) setPlayback(0);
  appState.isPlaying = !appState.isPlaying;
  if (appState.isPlaying) {
    appState.lastFrameAt = null;
    appState.rafId = requestAnimationFrame(tick);
  } else {
    stopPlayback();
  }
  render();
}

/* The mode buttons keep both dashboards on one page and simply move focus between them. */
function setMode(mode) {
  appState.mode = mode === "manual" ? "manual" : "replay";
  if (appState.mode !== "replay" && appState.isPlaying) stopPlayback();
  const target = document.getElementById(appState.mode === "manual" ? "manual-view" : "replay-view");
  target?.scrollIntoView({ behavior: "smooth", block: "start" });
  render();
}

/* The summary cards turn the exported metadata into a quick run overview. */
function renderSummary() {
  const target = document.getElementById("summary-grid");
  if (!appState.data) {
    target.innerHTML = `<div class="empty-state">No run data loaded yet.</div>`;
    return;
  }
  const finalEvent = appState.data.events.at(-1);
  const duration = finalEvent ? formatSeconds(finalEvent.elapsedMs) : "0.00s";
  const cards = [
    ["Users", appState.data.metadata.totalUsers, "Predefined engineers available to authenticate."],
    ["Max Sessions", appState.data.metadata.maxSessions, "Semaphore limit enforced by the session gate."],
    ["Reads / Writes", `${appState.data.summary.totalReads} / ${appState.data.summary.totalWrites}`, "Resource operations captured by the tracker."],
    ["Run Duration", duration, "Elapsed wall-clock time recorded for the visualization."],
  ];
  target.innerHTML = cards.map(([label, value, caption]) => `<article class="summary-card"><span>${label}</span><strong>${value}</strong><p>${caption}</p></article>`).join("");
}

/* Session-gate rendering is shared between replay snapshots and the manual sandbox. */
function renderGate(event, prefix = "") {
  const slots = document.getElementById(`${prefix}active-slots`);
  const queue = document.getElementById(`${prefix}waiting-queue`);
  const gateStatus = document.getElementById(`${prefix}gate-status`);
  const queueCount = document.getElementById(`${prefix}queue-count`);
  const maxSessions = appState.data?.metadata.maxSessions ?? 0;
  const activeUsers = event?.activeUsers ?? [];
  const waitingUsers = event?.waitingUsers ?? [];
  gateStatus.textContent = `${activeUsers.length} / ${maxSessions} active`;
  queueCount.textContent = `${waitingUsers.length} waiting`;
  slots.innerHTML = "";
  for (let i = 0; i < maxSessions; i++) {
    const userId = activeUsers[i];
    const user = userId ? usersById().get(userId) : null;
    slots.insertAdjacentHTML("beforeend", `<div class="slot-card ${user ? "occupied" : "idle"}"><span class="slot-label">${prefix ? "Manual " : ""}Slot ${i + 1}</span>${user ? `<strong>${user.username}</strong><p>User #${user.id} is inside the system.</p>` : `<strong>Available</strong><p>No active user in this slot.</p>`}</div>`);
  }
  queue.innerHTML = waitingUsers.length
    ? waitingUsers.map((id, index) => `<div class="token waiting"><small>Queue Position ${index + 1}</small><strong>${labelForUser(id)}</strong><small>ID ${id}</small></div>`).join("")
    : `<div class="empty-state">No queued users at this ${prefix ? "manual state" : "event"}.</div>`;
}

/* Resource rendering makes reader overlap and writer exclusivity immediately visible. */
function renderResource(event, ids = {}) {
  const readerLane = document.getElementById(ids.readerLane || "reader-lane");
  const writerLane = document.getElementById(ids.writerLane || "writer-lane");
  const readerCount = document.getElementById(ids.readerCount || "reader-count");
  const writerCount = document.getElementById(ids.writerCount || "writer-count");
  const resourceStatus = document.getElementById(ids.status || "resource-status");
  const readers = event?.readingUsers ?? [];
  const writer = event?.writingUser ?? null;
  readerCount.textContent = `${readers.length}`;
  writerCount.textContent = writer ? "1" : "0";
  resourceStatus.textContent = writer ? "Writer has exclusive control" : readers.length ? `${readers.length} reader${readers.length === 1 ? "" : "s"} sharing the file` : "Resource idle";
  readerLane.innerHTML = readers.length ? readers.map((id) => `<div class="token reading"><small>Reader</small><strong>${labelForUser(id)}</strong></div>`).join("") : `<div class="empty-state">No active readers at this moment.</div>`;
  writerLane.innerHTML = writer ? `<div class="token writing"><small>Writer lock engaged</small><strong>${labelForUser(writer)}</strong></div>` : `<div class="empty-state">No writer currently holds the file.</div>`;
}

/* User cards combine the exported totals with the selected snapshot state. */
function renderUsers(event) {
  const grid = document.getElementById("user-grid");
  const users = appState.data?.users ?? [];
  grid.innerHTML = users.map((user) => {
    const state = determineUserState(user.id, event);
    return `<article class="user-card"><div class="user-head"><div><span class="user-state-label">Engineer</span><strong>${user.username}</strong><p>User #${user.id}</p></div><span class="user-state ${state}">${state}</span></div><div class="user-metrics"><div class="mini-metric"><span>Total Reads</span><strong>${user.reads}</strong></div><div class="mini-metric"><span>Total Writes</span><strong>${user.writes}</strong></div></div></article>`;
  }).join("");
}

function renderSpotlight(event) {
  const target = document.getElementById("event-spotlight");
  if (!event) {
    target.innerHTML = `<div class="empty-state">No event selected.</div>`;
    return;
  }
  target.innerHTML = `<article class="spotlight-card"><div class="spotlight-top"><span class="spotlight-category category-${event.category}">${categoryLabel(event.category)}</span><span class="spotlight-index">#${event.id}</span></div><h3>${event.reason}</h3><div class="spotlight-meta"><div><span>Timestamp</span><strong>${event.timestamp}</strong></div><div><span>Elapsed</span><strong>${formatSeconds(event.elapsedMs)}</strong></div><div><span>Active / Waiting</span><strong>${event.activeUsers.length} / ${event.waitingUsers.length}</strong></div><div><span>Readers / Writer</span><strong>${event.readingUsers.length} / ${event.writingUser ? 1 : 0}</strong></div></div></article>`;
}

/* These checks make the key coursework concurrency rules explicit in the UI. */
function renderInvariants(event) {
  const target = document.getElementById("invariant-list");
  const maxSessions = appState.data?.metadata.maxSessions ?? 0;
  const checks = [
    ["Session limit respected", (event?.activeUsers.length ?? 0) <= maxSessions, `${event?.activeUsers.length ?? 0} active session(s) against a configured maximum of ${maxSessions}.`],
    ["Writer exclusivity respected", !((event?.writingUser != null) && (event?.readingUsers.length ?? 0) > 0), event?.writingUser != null ? "A writer is active, and reader overlap remains blocked." : "No writer is active, so reader overlap is permitted."],
    ["Queue visibility preserved", true, (event?.waitingUsers.length ?? 0) ? `${event.waitingUsers.length} user(s) are visible in the waiting queue.` : "No users are waiting at this snapshot."],
  ];
  target.innerHTML = checks.map(([title, pass, body]) => `<article class="invariant-item ${pass ? "pass" : "warn"}"><strong>${title}</strong><p>${body}</p></article>`).join("");
}

/* Show the events around the current playback position so transitions are easy to follow. */
function renderFeed() {
  const target = document.getElementById("event-feed");
  const events = appState.data?.events ?? [];
  if (!events.length) {
    target.innerHTML = `<div class="empty-state">Run the simulation to see the event history.</div>`;
    return;
  }
  const start = Math.max(0, appState.currentIndex - 4);
  const end = Math.min(events.length, appState.currentIndex + 7);
  target.innerHTML = events.slice(start, end).map((event, offset) => `<article class="event-item ${start + offset === appState.currentIndex ? "current" : ""}"><div class="event-top"><span class="event-kind">${categoryLabel(event.category)}</span><span class="event-time">${formatSeconds(event.elapsedMs)}</span></div><strong>${event.reason}</strong><p>Active ${event.activeUsers.length}, waiting ${event.waitingUsers.length}, readers ${event.readingUsers.length}, writer ${event.writingUser ? "engaged" : "idle"}.</p></article>`).join("");
}

function linePath(points) {
  return points.map((p, i) => `${i === 0 ? "M" : "L"} ${p.x.toFixed(2)} ${p.y.toFixed(2)}`).join(" ");
}

/* The SVG chart shows how pressure builds at the gate and around the shared file over time. */
function renderChart() {
  const svg = document.getElementById("timeline-chart");
  const data = appState.data;
  if (!data?.events?.length) {
    svg.innerHTML = "";
    return;
  }
  const width = 1000, height = 300, padding = { top: 18, right: 20, bottom: 32, left: 36 };
  const innerWidth = width - padding.left - padding.right;
  const innerHeight = height - padding.top - padding.bottom;
  const events = data.events;
  const maxElapsed = events.at(-1).elapsedMs || 1;
  const maxValue = Math.max(data.metadata.maxSessions, ...events.map((e) => Math.max(e.activeUsers.length, e.waitingUsers.length, e.readingUsers.length, e.writingUser ? 1 : 0)), 1);
  const scaleX = (ms) => padding.left + (ms / maxElapsed) * innerWidth;
  const scaleY = (v) => padding.top + innerHeight - (v / maxValue) * innerHeight;
  const series = {
    active: events.map((e) => ({ x: scaleX(e.elapsedMs), y: scaleY(e.activeUsers.length) })),
    waiting: events.map((e) => ({ x: scaleX(e.elapsedMs), y: scaleY(e.waitingUsers.length) })),
    reading: events.map((e) => ({ x: scaleX(e.elapsedMs), y: scaleY(e.readingUsers.length) })),
    writing: events.map((e) => ({ x: scaleX(e.elapsedMs), y: scaleY(e.writingUser ? 1 : 0) })),
  };
  const progressX = scaleX(currentEvent()?.elapsedMs ?? 0);
  const gridValues = Array.from({ length: maxValue + 1 }, (_, i) => i);
  svg.innerHTML = `${gridValues.map((v) => `<line class="chart-grid-line" x1="${padding.left}" y1="${scaleY(v)}" x2="${width - padding.right}" y2="${scaleY(v)}"></line><text class="chart-axis" x="8" y="${scaleY(v) + 4}">${v}</text>`).join("")}<text class="chart-label" x="${padding.left}" y="${height - 8}">0s</text><text class="chart-label" x="${width - padding.right - 42}" y="${height - 8}">${formatSeconds(maxElapsed)}</text><path class="chart-line active" d="${linePath(series.active)}"></path><path class="chart-line waiting" d="${linePath(series.waiting)}"></path><path class="chart-line reading" d="${linePath(series.reading)}"></path><path class="chart-line writing" d="${linePath(series.writing)}"></path><line class="chart-progress" x1="${progressX}" y1="${padding.top}" x2="${progressX}" y2="${height - padding.bottom}"></line>`;
}

/* Manual mode uses a separate state model so it never mutates the recorded replay dataset. */
function createManualState() {
  return { activeUsers: [], waitingUsers: [], readingUsers: [], writingUser: null, pendingWriters: [], log: [], tick: 0 };
}

function ensureManualState() {
  if (!appState.manual) appState.manual = createManualState();
}

/* Manual events are kept as a short rolling log for both the main panel and the side feed. */
function addManualLog(message, tone = "info") {
  ensureManualState();
  appState.manual.tick += 1;
  appState.manual.log.unshift({ id: appState.manual.tick, message, tone });
  appState.manual.log = appState.manual.log.slice(0, 10);
}

function manualMaxSessions() {
  return appState.data?.metadata.maxSessions ?? 4;
}

function manualUserState(userId) {
  ensureManualState();
  if (appState.manual.writingUser === userId) return "writing";
  if (appState.manual.readingUsers.includes(userId)) return "reading";
  if (appState.manual.waitingUsers.includes(userId)) return "waiting";
  if (appState.manual.activeUsers.includes(userId) || appState.manual.pendingWriters.includes(userId)) return "active";
  return "offline";
}

/* When an active slot becomes free, the oldest queued login is admitted first. */
function processManualWaitingQueue() {
  while (appState.manual.activeUsers.length < manualMaxSessions() && appState.manual.waitingUsers.length > 0) {
    const next = appState.manual.waitingUsers.shift();
    if (!appState.manual.activeUsers.includes(next)) {
      appState.manual.activeUsers.push(next);
      addManualLog(`${labelForUser(next)} left the login queue and entered the system.`, "success");
    }
  }
}

/* Writers only start when no writer is active and no readers still hold the file. */
function processManualWriterQueue() {
  if (appState.manual.writingUser != null || appState.manual.readingUsers.length > 0) return;
  while (appState.manual.pendingWriters.length > 0) {
    const next = appState.manual.pendingWriters.shift();
    if (appState.manual.activeUsers.includes(next)) {
      appState.manual.writingUser = next;
      addManualLog(`${labelForUser(next)} acquired the writer lock.`, "success");
      break;
    }
  }
}

function resetManualState() {
  appState.manual = createManualState();
  controls.manualWriteInput.value = "";
  addManualLog("Manual mode reset. The gate and file lock are both idle.", "info");
  render();
}

/* Poll the local API so the file preview reflects the latest saved shared-file contents. */
async function loadSharedFile() {
  try {
    const response = await fetch(`/api/shared-file?ts=${Date.now()}`, { cache: "no-store" });
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    const data = await response.json();
    appState.sharedFile = {
      content: data.content ?? "",
      size: data.size ?? 0,
      updatedAt: data.updatedAt ?? "",
      error: "",
    };
  } catch (error) {
    appState.sharedFile.error = error.message;
  }
  render();
}

/* Manual writes go through the local API so the dashboard updates the real shared file. */
async function saveSharedFileWrite(userId, rawText) {
  const text = rawText.trim();
  if (!text) {
    addManualLog("Type a file update before saving the shared file.", "warn");
    render();
    return false;
  }
  const user = usersById().get(userId);
  try {
    const response = await fetch("/api/shared-file", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        userId,
        username: user?.username ?? `user_${userId}`,
        content: text,
      }),
    });
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    controls.manualWriteInput.value = "";
    addManualLog(`${labelForUser(userId)} committed a write to ProductSpecification.txt.`, "success");
    await loadSharedFile();
    return true;
  } catch (error) {
    appState.sharedFile.error = error.message;
    addManualLog(`Shared file write failed: ${error.message}`, "warn");
    render();
    return false;
  }
}

/* Manual login models the same bounded-session rule enforced by the C++ semaphore. */
function attemptManualLogin(userId) {
  ensureManualState();
  if (appState.manual.activeUsers.includes(userId)) return addManualLog(`${labelForUser(userId)} is already logged in.`, "warn"), render();
  if (appState.manual.waitingUsers.includes(userId)) return addManualLog(`${labelForUser(userId)} is already waiting for a session slot.`, "warn"), render();
  if (appState.manual.activeUsers.length < manualMaxSessions()) {
    appState.manual.activeUsers.push(userId);
    addManualLog(`${labelForUser(userId)} logged in immediately.`, "success");
  } else {
    appState.manual.waitingUsers.push(userId);
    addManualLog(`${labelForUser(userId)} attempted login and was queued by the semaphore gate.`, "info");
  }
  render();
}

/* Logging out also releases any read or write state that user still held in the sandbox. */
function manualLogout(userId) {
  ensureManualState();
  const waitIndex = appState.manual.waitingUsers.indexOf(userId);
  if (waitIndex !== -1) {
    appState.manual.waitingUsers.splice(waitIndex, 1);
    addManualLog(`${labelForUser(userId)} left the waiting queue before login.`, "info");
    return render();
  }
  const activeIndex = appState.manual.activeUsers.indexOf(userId);
  if (activeIndex === -1) return addManualLog(`${labelForUser(userId)} is not currently logged in.`, "warn"), render();
  appState.manual.activeUsers.splice(activeIndex, 1);
  appState.manual.readingUsers = appState.manual.readingUsers.filter((id) => id !== userId);
  appState.manual.pendingWriters = appState.manual.pendingWriters.filter((id) => id !== userId);
  if (appState.manual.writingUser === userId) appState.manual.writingUser = null;
  addManualLog(`${labelForUser(userId)} logged out and released any held resources.`, "success");
  processManualWaitingQueue();
  processManualWriterQueue();
  render();
}

/* New reads are blocked when a writer is active or already queued, mirroring writer priority. */
function manualStartRead(userId) {
  ensureManualState();
  if (!appState.manual.activeUsers.includes(userId)) return addManualLog(`${labelForUser(userId)} cannot read before logging in.`, "warn"), render();
  if (appState.manual.readingUsers.includes(userId)) return addManualLog(`${labelForUser(userId)} is already reading.`, "warn"), render();
  if (appState.manual.writingUser === userId) return addManualLog(`${labelForUser(userId)} must stop writing before starting a read.`, "warn"), render();
  if (appState.manual.writingUser != null) return addManualLog(`${labelForUser(userId)} cannot read while another writer holds the lock.`, "warn"), render();
  if (appState.manual.pendingWriters.length > 0) return addManualLog(`${labelForUser(userId)} is blocked because a writer is already queued.`, "warn"), render();
  appState.manual.readingUsers.push(userId);
  addManualLog(`${labelForUser(userId)} started reading the shared file.`, "success");
  loadSharedFile();
  render();
}

function manualStopRead(userId) {
  ensureManualState();
  if (!appState.manual.readingUsers.includes(userId)) return addManualLog(`${labelForUser(userId)} is not currently reading.`, "warn"), render();
  appState.manual.readingUsers = appState.manual.readingUsers.filter((id) => id !== userId);
  addManualLog(`${labelForUser(userId)} stopped reading the shared file.`, "info");
  processManualWriterQueue();
  render();
}

/* Writes either start immediately on an idle file or join the exclusive writer queue. */
function manualStartWrite(userId) {
  ensureManualState();
  if (!appState.manual.activeUsers.includes(userId)) return addManualLog(`${labelForUser(userId)} cannot write before logging in.`, "warn"), render();
  if (appState.manual.writingUser === userId) return addManualLog(`${labelForUser(userId)} already holds the writer lock.`, "warn"), render();
  if (appState.manual.readingUsers.includes(userId)) return addManualLog(`${labelForUser(userId)} must stop reading before requesting a write.`, "warn"), render();
  if (appState.manual.pendingWriters.includes(userId)) return addManualLog(`${labelForUser(userId)} is already waiting for the writer lock.`, "warn"), render();
  if (appState.manual.writingUser == null && appState.manual.readingUsers.length === 0) {
    appState.manual.writingUser = userId;
    addManualLog(`${labelForUser(userId)} acquired the writer lock immediately.`, "success");
    loadSharedFile();
  } else {
    appState.manual.pendingWriters.push(userId);
    addManualLog(`${labelForUser(userId)} requested a write and is now queued for exclusive access.`, "info");
  }
  render();
}

/* Releasing a write can optionally commit the draft first, then promote the next waiting writer. */
async function manualStopWrite(userId) {
  ensureManualState();
  if (appState.manual.writingUser !== userId) return addManualLog(`${labelForUser(userId)} does not currently hold the writer lock.`, "warn"), render();
  const draft = controls.manualWriteInput.value;
  if (draft.trim()) {
    const saved = await saveSharedFileWrite(userId, draft);
    if (!saved) return;
  }
  appState.manual.writingUser = null;
  addManualLog(`${labelForUser(userId)} released the writer lock.`, "info");
  processManualWriterQueue();
  render();
}

async function commitManualWriterDraft() {
  ensureManualState();
  const writerId = appState.manual.writingUser;
  if (writerId == null) {
    addManualLog("No user currently holds the writer lock.", "warn");
    render();
    return;
  }
  await saveSharedFileWrite(writerId, controls.manualWriteInput.value);
}

function renderManualState() {
  const status = document.getElementById("manual-status");
  if (!appState.data) {
    status.textContent = "Manual mode needs runtime data before it can initialize.";
    return;
  }
  ensureManualState();
  status.textContent = `Manual mode: ${appState.manual.activeUsers.length} active, ${appState.manual.waitingUsers.length} waiting, ${appState.manual.readingUsers.length} reading, ${appState.manual.writingUser != null ? "writer engaged" : "writer idle"}.`;
  renderGate({ activeUsers: appState.manual.activeUsers, waitingUsers: appState.manual.waitingUsers }, "manual-");
  renderResource(
    { readingUsers: appState.manual.readingUsers, writingUser: appState.manual.writingUser },
    {
      readerLane: "manual-reader-lane",
      writerLane: "manual-writer-lane",
      readerCount: "manual-reader-count",
      writerCount: "manual-writer-count",
      status: "manual-resource-status",
    }
  );
  document.getElementById("manual-writer-queue-count").textContent = `${appState.manual.pendingWriters.length} waiting`;
  document.getElementById("manual-writer-queue").innerHTML = appState.manual.pendingWriters.length
    ? appState.manual.pendingWriters.map((id, index) => `<div class="token waiting"><small>Writer Queue ${index + 1}</small><strong>${labelForUser(id)}</strong><small>Waiting for exclusive access</small></div>`).join("")
    : `<div class="empty-state">No pending writers.</div>`;
  document.getElementById("manual-users").innerHTML = (appState.data.users ?? []).map((user) => `<article class="manual-card"><div class="manual-card-head"><div><span class="user-state-label">Manual Control</span><strong>${user.username}</strong><p>User #${user.id}</p></div><span class="user-state ${manualUserState(user.id)}">${manualUserState(user.id)}</span></div><div class="manual-actions"><button class="action-button small primary manual-action" data-action="login" data-user="${user.id}" type="button">Attempt Login</button><button class="action-button small ghost manual-action" data-action="logout" data-user="${user.id}" type="button">Logout</button><button class="action-button small ghost manual-action" data-action="read-start" data-user="${user.id}" type="button">Read Start</button><button class="action-button small ghost manual-action" data-action="read-stop" data-user="${user.id}" type="button">Read Stop</button><button class="action-button small ghost manual-action" data-action="write-start" data-user="${user.id}" type="button">Write Request</button><button class="action-button small ghost manual-action" data-action="write-stop" data-user="${user.id}" type="button">Write Stop</button></div></article>`).join("");
  document.getElementById("manual-log").innerHTML = appState.manual.log.length
    ? appState.manual.log.map((entry) => `<article class="manual-log-item ${entry.tone}"><strong>Manual Event #${entry.id}</strong><span>${entry.message}</span></article>`).join("")
    : `<div class="empty-state">Manual actions will appear here.</div>`;
  document.getElementById("manual-log-side").innerHTML = appState.manual.log.length
    ? appState.manual.log.map((entry) => `<article class="event-item ${entry.tone}"><div class="event-top"><span class="event-kind">Manual Event</span><span class="event-time">#${entry.id}</span></div><strong>${entry.message}</strong><p>${entry.tone === "success" ? "The sandbox accepted the action and updated the shared state." : entry.tone === "warn" ? "The action was blocked to preserve a concurrency rule." : "The sandbox recorded a neutral state transition."}</p></article>`).join("")
    : `<div class="empty-state">Manual actions will appear here.</div>`;
  renderManualFilePanel();
}

/* The file panel connects manual lock state to the live file preview and editor controls. */
function renderManualFilePanel() {
  const writerId = appState.manual?.writingUser ?? null;
  const writerOwner = document.getElementById("manual-writer-owner");
  const fileStatus = document.getElementById("manual-file-status");
  const fileMeta = document.getElementById("manual-file-meta");
  const fileContent = document.getElementById("manual-file-content");
  const draftInput = controls.manualWriteInput;
  const saveButton = controls.manualWriteSave;
  const readers = appState.manual?.readingUsers ?? [];
  writerOwner.textContent = writerId != null ? `${labelForUser(writerId)} holds the lock` : "No writer active";
  draftInput.disabled = writerId == null;
  saveButton.disabled = writerId == null;
  draftInput.placeholder = writerId != null
    ? `Type the update that ${labelForUser(writerId)} will commit to ProductSpecification.txt.`
    : "Acquire the writer lock, then type an update for ProductSpecification.txt.";
  if (appState.sharedFile.error) {
    fileStatus.textContent = `Shared file unavailable: ${appState.sharedFile.error}`;
    fileMeta.textContent = "File read error";
    fileContent.textContent = "Could not load ProductSpecification.txt from the local API.";
    return;
  }
  fileStatus.textContent = `${readers.length} reader${readers.length === 1 ? "" : "s"} inspecting the file. ${writerId != null ? `${labelForUser(writerId)} can commit updates right now.` : "No one currently has write access."}`;
  fileMeta.textContent = appState.sharedFile.updatedAt
    ? `${appState.sharedFile.size} bytes | updated ${appState.sharedFile.updatedAt}`
    : `${appState.sharedFile.size} bytes`;
  fileContent.textContent = appState.sharedFile.content || "ProductSpecification.txt is currently empty.";
}

/* The hero panel doubles as the high-level explanation area for whichever mode is active. */
function renderHero(event) {
  const phase = appState.mode === "manual" ? manualPhase() : phaseFromEvent(event);
  const finalEvent = appState.data?.events?.at(-1);
  document.getElementById("data-source").textContent = "frontend/data/conres_run.json";
  document.getElementById("data-caption").textContent = appState.mode === "manual"
    ? "Manual mode reuses the same runtime export for user identities and semaphore limits."
    : finalEvent ? `${appState.data.metadata.totalEvents} events captured across ${formatSeconds(finalEvent.elapsedMs)}.` : "No events captured.";
  document.getElementById("phase-title").textContent = phase.title;
  document.getElementById("phase-caption").textContent = phase.caption;
}

function renderControls(event) {
  const total = appState.data?.events?.length ?? 0;
  controls.playToggle.textContent = appState.isPlaying ? "Pause" : "Play";
  controls.scrubber.max = `${Math.max(total - 1, 0)}`;
  controls.scrubber.value = `${appState.currentIndex}`;
  document.getElementById("event-counter").textContent = `Event ${total ? appState.currentIndex + 1 : 0} / ${total}`;
  document.getElementById("event-time").textContent = event ? `t = ${formatSeconds(event.elapsedMs)}` : "t = 0.00s";
}

function renderMode() {
  const isReplay = appState.mode === "replay";
  controls.replayMode.classList.toggle("is-active", isReplay);
  controls.manualMode.classList.toggle("is-active", !isReplay);
  controls.replayMode.setAttribute("aria-pressed", isReplay ? "true" : "false");
  controls.manualMode.setAttribute("aria-pressed", isReplay ? "false" : "true");
}

/* A full render refreshes both replay and manual sections from the latest app state. */
function render() {
  const event = currentEvent();
  renderMode();
  renderHero(event);
  renderControls(event);
  renderSummary();
  renderGate(event);
  renderResource(event);
  renderUsers(event);
  renderSpotlight(event);
  renderInvariants(event);
  renderFeed();
  renderChart();
  renderManualState();
}

/* Replay mode depends on the exported JSON file produced by the native runtime. */
async function loadRun() {
  try {
    const response = await fetch(`data/conres_run.json?ts=${Date.now()}`, { cache: "no-store" });
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    appState.data = await response.json();
    appState.currentIndex = 0;
    appState.playbackTime = 0;
    resetManualState();
  } catch (error) {
    document.getElementById("data-source").textContent = "Data file missing";
    document.getElementById("data-caption").textContent = "Run main.exe or launch_frontend.ps1 to generate frontend/data/conres_run.json.";
    document.getElementById("phase-title").textContent = "Awaiting Data";
    document.getElementById("phase-caption").textContent = "The frontend is ready, but it needs a fresh simulation export.";
    document.getElementById("summary-grid").innerHTML = `<div class="empty-state">Could not load run data: ${error.message}</div>`;
    document.getElementById("event-feed").innerHTML = `<div class="empty-state">No event feed available yet.</div>`;
    document.getElementById("event-spotlight").innerHTML = `<div class="empty-state">No spotlight event available yet.</div>`;
    document.getElementById("invariant-list").innerHTML = `<div class="empty-state">No invariants can be checked until the dataset is loaded.</div>`;
    document.getElementById("active-slots").innerHTML = `<div class="empty-state">No session state available.</div>`;
    document.getElementById("waiting-queue").innerHTML = `<div class="empty-state">No queue data available.</div>`;
    document.getElementById("reader-lane").innerHTML = `<div class="empty-state">No resource data available.</div>`;
    document.getElementById("writer-lane").innerHTML = `<div class="empty-state">No resource data available.</div>`;
    document.getElementById("user-grid").innerHTML = `<div class="empty-state">No user data available.</div>`;
    document.getElementById("timeline-chart").innerHTML = "";
    document.getElementById("manual-status").textContent = "Manual mode is unavailable until simulation data is loaded.";
    document.getElementById("manual-users").innerHTML = `<div class="empty-state">No manual controls available yet.</div>`;
    document.getElementById("manual-log").innerHTML = `<div class="empty-state">No manual log available yet.</div>`;
    document.getElementById("manual-log-side").innerHTML = `<div class="empty-state">No manual log available yet.</div>`;
    document.getElementById("manual-file-status").textContent = "Shared file view is unavailable until simulation data is loaded.";
    document.getElementById("manual-file-meta").textContent = "No file data";
    document.getElementById("manual-file-content").textContent = "No shared file data available yet.";
  }
}

function handleManualAction(action, userId) {
  if (action === "login") return attemptManualLogin(userId);
  if (action === "logout") return manualLogout(userId);
  if (action === "read-start") return manualStartRead(userId);
  if (action === "read-stop") return manualStopRead(userId);
  if (action === "write-start") return manualStartWrite(userId);
  if (action === "write-stop") return manualStopWrite(userId);
}

/* Wire once, then let state changes drive everything else. */
function wireControls() {
  controls.playToggle.addEventListener("click", togglePlayback);
  controls.resetButton.addEventListener("click", () => { stopPlayback(); setPlayback(0); });
  controls.prevButton.addEventListener("click", () => { stopPlayback(); setPlayback(appState.currentIndex - 1); });
  controls.nextButton.addEventListener("click", () => { stopPlayback(); setPlayback(appState.currentIndex + 1); });
  controls.scrubber.addEventListener("input", (event) => { stopPlayback(); setPlayback(Number(event.target.value)); });
  controls.speedSelect.addEventListener("change", (event) => { appState.speed = Number(event.target.value); });
  controls.replayMode.addEventListener("click", () => setMode("replay"));
  controls.manualMode.addEventListener("click", () => setMode("manual"));
  controls.manualReset.addEventListener("click", () => resetManualState());
  controls.manualFileRefresh.addEventListener("click", () => loadSharedFile());
  controls.manualWriteSave.addEventListener("click", () => commitManualWriterDraft());
  document.addEventListener("click", (event) => {
    const button = event.target.closest(".manual-action");
    if (!button) return;
    handleManualAction(button.dataset.action, Number(button.dataset.user));
  });
}

wireControls();
loadRun();
loadSharedFile();
/* Keep the shared-file panel fresh during manual demonstrations. */
setInterval(() => {
  if (appState.data) loadSharedFile();
}, 3000);

/*
 * AKuarium dashboard
 * Reads public ThingSpeak Field 1 data only. No credentials are stored here.
 */

const THINGSPEAK_API = "https://api.thingspeak.com";
const FEED_PAGE_SIZE = 8000;
const AUTO_REFRESH_MS = 5 * 60 * 1000;

const state = {
  channelId: null,
  channel: null,
  readings: [],
  totalFeedEntries: 0,
  invalidFeedEntries: 0,
  range: "all",
  chart: null,
  chartSpan: 0,
  autoRefresh: true,
  refreshTimer: null,
  abortController: null,
  lastRefreshed: null,
};

const elements = {
  form: document.querySelector("#channel-form"),
  channelId: document.querySelector("#channel-id"),
  loadButton: document.querySelector("#load-button"),
  status: document.querySelector("#app-status"),
  dashboard: document.querySelector("#dashboard"),
  channelName: document.querySelector("#channel-name"),
  channelDescription: document.querySelector("#channel-description"),
  detailChannelId: document.querySelector("#detail-channel-id"),
  detailFieldName: document.querySelector("#detail-field-name"),
  detailUpdated: document.querySelector("#detail-updated"),
  latestTime: document.querySelector("#latest-reading-time"),
  currentTemperature: document.querySelector("#current-temperature"),
  minimumTemperature: document.querySelector("#minimum-temperature"),
  maximumTemperature: document.querySelector("#maximum-temperature"),
  averageTemperature: document.querySelector("#average-temperature"),
  readingCount: document.querySelector("#reading-count"),
  rangeButtons: document.querySelectorAll(".range-button"),
  chartReadingCount: document.querySelector("#chart-reading-count"),
  chartCanvas: document.querySelector("#temperature-chart"),
  chartEmptyState: document.querySelector("#chart-empty-state"),
  chartSummary: document.querySelector("#chart-summary"),
  resetZoom: document.querySelector("#reset-zoom"),
  refreshTime: document.querySelector("#refresh-time"),
  autoRefreshToggle: document.querySelector("#auto-refresh-toggle"),
  autoRefreshLabel: document.querySelector("#auto-refresh-label"),
};

class DashboardError extends Error {
  constructor(kind, message) {
    super(message);
    this.name = "DashboardError";
    this.kind = kind;
  }
}

elements.form.addEventListener("submit", (event) => {
  event.preventDefault();
  const channelId = elements.channelId.value.trim();

  if (!/^\d+$/.test(channelId)) {
    showStatus("error", "Enter a valid numeric ThingSpeak Channel ID.");
    elements.channelId.focus();
    return;
  }

  loadChannel(channelId);
});

elements.rangeButtons.forEach((button) => {
  button.addEventListener("click", () => {
    state.range = button.dataset.range;
    updateRangeButtons();
    renderChart();
  });
});

elements.resetZoom.addEventListener("click", () => {
  if (state.chart && typeof state.chart.resetZoom === "function") {
    state.chart.resetZoom();
  }
});

elements.autoRefreshToggle.addEventListener("click", () => {
  state.autoRefresh = !state.autoRefresh;
  updateAutoRefreshControl();
  scheduleAutoRefresh();
  showStatus(
    "success",
    state.autoRefresh
      ? "Auto refresh is on. The latest readings will be checked every five minutes."
      : "Auto refresh is off. Your current data will stay on screen until you load the channel again."
  );
});

/** Fetch a channel's metadata and every available Field 1 feed page. */
async function loadChannel(channelId) {
  if (state.abortController) {
    state.abortController.abort();
  }

  const controller = new AbortController();
  state.abortController = controller;
  setLoading(true);
  showStatus("loading", "Connecting to ThingSpeak and loading channel information…");

  try {
    const channel = await requestJson(`/channels/${channelId}.json`, controller.signal);
    validateChannel(channel);
    renderChannelInfo(channel, channelId);
    elements.dashboard.hidden = false;

    const result = await fetchAllHistory(channelId, controller.signal);
    if (controller.signal.aborted) return;

    state.channelId = channelId;
    state.channel = { ...channel, ...result.channel };
    state.readings = result.readings;
    state.totalFeedEntries = result.totalFeedEntries;
    state.invalidFeedEntries = result.invalidFeedEntries;
    state.lastRefreshed = new Date();
    state.range = "all";

    updateRangeButtons();
    renderChannelInfo(state.channel, channelId);
    renderRefreshTime();

    if (state.readings.length === 0) {
      clearMetrics();
      destroyChart();
      showStatus(
        "error",
        "This channel loaded, but it does not contain usable Field 1 temperature data. Check that Field 1 has public readings."
      );
      elements.chartReadingCount.textContent = "0 readings represented";
      elements.chartEmptyState.hidden = false;
      elements.chartSummary.textContent = "No valid numeric Field 1 values were returned by ThingSpeak.";
      scheduleAutoRefresh();
      return;
    }

    renderMetrics();
    renderChart();
    scheduleAutoRefresh();

    const ignoredMessage = state.invalidFeedEntries
      ? ` ${state.invalidFeedEntries.toLocaleString()} empty or invalid Field 1 entr${state.invalidFeedEntries === 1 ? "y was" : "ies were"} ignored.`
      : "";
    showStatus(
      "success",
      `Loaded ${state.readings.length.toLocaleString()} valid temperature reading${state.readings.length === 1 ? "" : "s"} from this public channel.${ignoredMessage}`
    );
  } catch (error) {
    if (error.name === "AbortError") return;
    handleLoadError(error);
  } finally {
    if (state.abortController === controller) {
      state.abortController = null;
      setLoading(false);
    }
  }
}

/**
 * ThingSpeak caps a feed request at 8,000 records. Starting with the newest
 * page, this walks backward using the oldest timestamp in each response so the
 * All Time view is not limited to a single 8,000-record API response.
 */
async function fetchAllHistory(channelId, signal) {
  const allFeeds = [];
  let endDate = null;
  let page = 0;
  let previousCursor = null;
  let channel = null;

  while (true) {
    const params = new URLSearchParams({ results: String(FEED_PAGE_SIZE) });
    if (endDate) params.set("end", toThingSpeakDate(endDate));

    const payload = await requestJson(`/channels/${channelId}/feeds.json?${params.toString()}`, signal);
    const feeds = Array.isArray(payload.feeds) ? payload.feeds : [];
    channel = channel || payload.channel || null;

    if (feeds.length === 0) break;
    allFeeds.push(...feeds);
    page += 1;

    if (feeds.length < FEED_PAGE_SIZE) break;

    const oldestTimestamp = feeds.reduce((oldest, feed) => {
      const timestamp = Date.parse(feed.created_at);
      return Number.isFinite(timestamp) && timestamp < oldest ? timestamp : oldest;
    }, Number.POSITIVE_INFINITY);

    if (!Number.isFinite(oldestTimestamp)) {
      throw new DashboardError("api", "ThingSpeak returned a full page with unusable timestamps.");
    }

    const nextCursor = oldestTimestamp - 1000;
    if (nextCursor === previousCursor) {
      throw new DashboardError("api", "ThingSpeak returned a repeated history page.");
    }

    previousCursor = nextCursor;
    endDate = new Date(nextCursor);
    showStatus(
      "loading",
      `Loaded ${allFeeds.length.toLocaleString()} entries so far. Retrieving earlier channel history…`
    );
  }

  const feedsById = new Map();
  allFeeds.forEach((feed) => {
    const key = feed.entry_id ?? `${feed.created_at}|${feed.field1}`;
    feedsById.set(String(key), feed);
  });

  const normalized = normalizeFeeds([...feedsById.values()]);
  return {
    ...normalized,
    totalFeedEntries: feedsById.size,
    pagesLoaded: page,
    channel,
  };
}

/** Retrieve only the newest API page during automatic refresh, then merge it into cached history. */
async function refreshLatestReadings() {
  if (!state.channelId || state.abortController) return;

  const controller = new AbortController();
  state.abortController = controller;
  setLoading(true);
  showStatus("loading", "Refreshing the latest ThingSpeak readings…");

  try {
    const payload = await requestJson(
      `/channels/${state.channelId}/feeds.json?results=${FEED_PAGE_SIZE}`,
      controller.signal
    );
    const latest = normalizeFeeds(Array.isArray(payload.feeds) ? payload.feeds : []);
    const merged = new Map(state.readings.map((reading) => [reading.key, reading]));
    latest.readings.forEach((reading) => merged.set(reading.key, reading));

    state.readings = [...merged.values()].sort((a, b) => a.timestamp - b.timestamp);
    state.totalFeedEntries = Math.max(state.totalFeedEntries, state.readings.length);
    state.invalidFeedEntries = Math.max(state.invalidFeedEntries, latest.invalidFeedEntries);
    state.channel = payload.channel || state.channel;
    state.lastRefreshed = new Date();

    renderChannelInfo(state.channel, state.channelId);
    renderMetrics();
    renderChart();
    renderRefreshTime();
    showStatus("success", "Latest public ThingSpeak readings refreshed.");
  } catch (error) {
    if (error.name !== "AbortError") {
      showStatus("error", `${messageForError(error)} The displayed readings have not been changed.`);
    }
  } finally {
    if (state.abortController === controller) {
      state.abortController = null;
      setLoading(false);
      scheduleAutoRefresh();
    }
  }
}

function normalizeFeeds(feeds) {
  let invalidFeedEntries = 0;
  const readings = [];

  feeds.forEach((feed) => {
    const timestamp = Date.parse(feed.created_at);
    const fieldValue = typeof feed.field1 === "string" ? feed.field1.trim() : feed.field1;
    const temperature = fieldValue === "" || fieldValue === null || fieldValue === undefined
      ? Number.NaN
      : Number(fieldValue);

    if (!Number.isFinite(timestamp) || !Number.isFinite(temperature)) {
      invalidFeedEntries += 1;
      return;
    }

    readings.push({
      key: String(feed.entry_id ?? `${feed.created_at}|${feed.field1}`),
      timestamp,
      temperature,
    });
  });

  readings.sort((a, b) => a.timestamp - b.timestamp);
  return { readings, invalidFeedEntries };
}

async function requestJson(path, signal) {
  let response;

  try {
    response = await fetch(`${THINGSPEAK_API}${path}`, {
      headers: { Accept: "application/json" },
      signal,
    });
  } catch (error) {
    if (error.name === "AbortError") throw error;
    throw new DashboardError("network", "Unable to reach ThingSpeak.");
  }

  if (!response.ok) {
    if (response.status === 401 || response.status === 403) {
      throw new DashboardError("private", "This channel is private or does not allow public reads.");
    }
    if (response.status === 404) {
      throw new DashboardError("missing", "ThingSpeak could not find that channel.");
    }
    if (response.status === 429) {
      throw new DashboardError("rate-limit", "ThingSpeak is asking us to slow down. Try again shortly.");
    }
    throw new DashboardError("api", `ThingSpeak responded with status ${response.status}.`);
  }

  try {
    return await response.json();
  } catch {
    throw new DashboardError("api", "ThingSpeak returned an unexpected response.");
  }
}

function validateChannel(channel) {
  if (!channel || typeof channel !== "object" || !channel.id) {
    throw new DashboardError("missing", "ThingSpeak did not return a public channel.");
  }
}

function renderChannelInfo(channel, channelId) {
  elements.channelName.textContent = channel.name?.trim() || "Unnamed ThingSpeak channel";
  elements.channelDescription.textContent = channel.description?.trim() || "No channel description was provided.";
  elements.detailChannelId.textContent = String(channel.id || channelId);
  elements.detailFieldName.textContent = channel.field1?.trim() || "Field 1 (no name provided)";
  elements.detailUpdated.textContent = formatTimestamp(channel.updated_at) || "Not available";
}

function renderMetrics() {
  const latest = state.readings.at(-1);
  const statistics = state.readings.reduce(
    (summary, reading) => ({
      minimum: Math.min(summary.minimum, reading.temperature),
      maximum: Math.max(summary.maximum, reading.temperature),
      total: summary.total + reading.temperature,
    }),
    { minimum: Number.POSITIVE_INFINITY, maximum: Number.NEGATIVE_INFINITY, total: 0 }
  );
  const average = statistics.total / state.readings.length;

  elements.currentTemperature.innerHTML = `${formatTemperature(latest.temperature)} <span>°C</span>`;
  elements.minimumTemperature.innerHTML = `${formatTemperature(statistics.minimum)} <span>°C</span>`;
  elements.maximumTemperature.innerHTML = `${formatTemperature(statistics.maximum)} <span>°C</span>`;
  elements.averageTemperature.innerHTML = `${formatTemperature(average)} <span>°C</span>`;
  elements.readingCount.textContent = state.readings.length.toLocaleString();
  elements.latestTime.textContent = `Latest reading: ${formatTimestamp(latest.timestamp)}`;
}

function clearMetrics() {
  [
    elements.currentTemperature,
    elements.minimumTemperature,
    elements.maximumTemperature,
    elements.averageTemperature,
  ].forEach((element) => {
    element.innerHTML = `— <span>°C</span>`;
  });
  elements.readingCount.textContent = "0";
  elements.latestTime.textContent = "Latest reading: not available";
}

function renderChart() {
  const readings = getReadingsForRange();
  elements.chartReadingCount.textContent = `${readings.length.toLocaleString()} reading${readings.length === 1 ? "" : "s"} represented`;
  elements.chartEmptyState.hidden = readings.length > 0;
  elements.resetZoom.disabled = readings.length === 0;

  if (readings.length === 0) {
    destroyChart();
    elements.chartSummary.textContent = "No valid readings exist in this selected time range.";
    return;
  }

  if (!window.Chart) {
    destroyChart();
    elements.chartSummary.textContent = "The chart library could not load. Temperature statistics are still available.";
    return;
  }

  const points = readings.map((reading) => ({ x: reading.timestamp, y: reading.temperature }));
  const visualDataNote = readings.length > 1500
    ? " The chart uses visual decimation for smooth rendering while keeping every original reading loaded."
    : "";
  elements.chartSummary.textContent = `Hover a point for its exact timestamp and temperature. Use Ctrl + mouse wheel or pinch to zoom; drag to pan.${visualDataNote}`;

  destroyChart();
  state.chartSpan = readings.at(-1).timestamp - readings[0].timestamp;
  state.chart = new window.Chart(elements.chartCanvas, {
    type: "line",
    data: {
      datasets: [{
        label: "Temperature",
        data: points,
        parsing: false,
        borderColor: "#62e2ee",
        backgroundColor: "rgba(98, 226, 238, 0.17)",
        borderWidth: 2,
        pointRadius: 0,
        pointHitRadius: 9,
        pointHoverRadius: 4,
        pointHoverBackgroundColor: "#b5f9ff",
        pointHoverBorderColor: "#123a4a",
        tension: 0.16,
        fill: true,
      }],
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      animation: false,
      normalized: true,
      interaction: { mode: "nearest", axis: "x", intersect: false },
      layout: { padding: { top: 4, right: 10, bottom: 2, left: 2 } },
      plugins: {
        legend: { display: false },
        decimation: {
          enabled: points.length > 1500,
          algorithm: "lttb",
          samples: 1200,
        },
        tooltip: {
          displayColors: false,
          backgroundColor: "rgba(3, 20, 31, 0.96)",
          borderColor: "rgba(148, 245, 255, 0.48)",
          borderWidth: 1,
          padding: 11,
          titleColor: "#dffbff",
          bodyColor: "#87eaf4",
          callbacks: {
            title: (items) => formatTimestamp(items[0].parsed.x),
            label: (item) => `${formatTemperature(item.parsed.y)} °C`,
          },
        },
        zoom: {
          pan: { enabled: true, mode: "x", modifierKey: null },
          zoom: {
            wheel: { enabled: true, modifierKey: "ctrl" },
            pinch: { enabled: true },
            mode: "x",
          },
        },
      },
      scales: {
        x: {
          type: "linear",
          grid: { color: "rgba(138, 211, 224, 0.09)" },
          border: { color: "rgba(138, 211, 224, 0.22)" },
          ticks: {
            color: "#92b5c0",
            maxTicksLimit: 7,
            maxRotation: 0,
            callback: (value) => formatAxisTime(Number(value)),
          },
          title: { display: true, text: "Time / Date", color: "#a9cbd4", font: { size: 12, weight: "600" } },
        },
        y: {
          title: { display: true, text: "Temperature (°C)", color: "#a9cbd4", font: { size: 12, weight: "600" } },
          grid: { color: "rgba(138, 211, 224, 0.1)" },
          border: { color: "rgba(138, 211, 224, 0.22)" },
          ticks: {
            color: "#92b5c0",
            callback: (value) => `${formatTemperature(Number(value))}°`,
          },
        },
      },
    },
  });
}

function getReadingsForRange() {
  if (state.range === "all") return state.readings;

  const millisecondsByRange = {
    "24h": 24 * 60 * 60 * 1000,
    "7d": 7 * 24 * 60 * 60 * 1000,
    "30d": 30 * 24 * 60 * 60 * 1000,
  };
  const cutoff = Date.now() - millisecondsByRange[state.range];
  return state.readings.filter((reading) => reading.timestamp >= cutoff);
}

function destroyChart() {
  if (state.chart) {
    state.chart.destroy();
    state.chart = null;
  }
  state.chartSpan = 0;
}

function updateRangeButtons() {
  elements.rangeButtons.forEach((button) => {
    const isActive = button.dataset.range === state.range;
    button.classList.toggle("is-active", isActive);
    button.setAttribute("aria-pressed", String(isActive));
  });
}

function updateAutoRefreshControl() {
  elements.autoRefreshToggle.classList.toggle("is-on", state.autoRefresh);
  elements.autoRefreshToggle.setAttribute("aria-pressed", String(state.autoRefresh));
  elements.autoRefreshLabel.textContent = `Auto refresh: ${state.autoRefresh ? "ON" : "OFF"}`;
}

function scheduleAutoRefresh() {
  window.clearTimeout(state.refreshTimer);
  state.refreshTimer = null;

  if (state.autoRefresh && state.channelId) {
    state.refreshTimer = window.setTimeout(refreshLatestReadings, AUTO_REFRESH_MS);
  }
}

function renderRefreshTime() {
  elements.refreshTime.textContent = `Last refreshed: ${formatTimestamp(state.lastRefreshed)}`;
}

function setLoading(isLoading) {
  elements.loadButton.classList.toggle("is-loading", isLoading);
  elements.loadButton.disabled = isLoading;
  elements.channelId.disabled = isLoading;
  elements.form.setAttribute("aria-busy", String(isLoading));
}

function showStatus(type, message) {
  const icons = { neutral: "⌁", loading: "◌", success: "✓", error: "!" };
  elements.status.className = `notice notice--${type}`;
  elements.status.innerHTML = `<span class="notice__icon" aria-hidden="true">${icons[type] || icons.neutral}</span><p></p>`;
  elements.status.querySelector("p").textContent = message;
}

function handleLoadError(error) {
  const message = messageForError(error);
  showStatus("error", message);
}

function messageForError(error) {
  if (error instanceof DashboardError) {
    if (error.kind === "missing") {
      return "Channel not found. Check that the ThingSpeak Channel ID is correct.";
    }
    if (error.kind === "private") {
      return "This channel is private or unauthorized. Use a public ThingSpeak channel with read access.";
    }
    if (error.kind === "rate-limit") {
      return "ThingSpeak is temporarily rate-limiting requests. Please try again in a moment.";
    }
    if (error.kind === "network") {
      return "Unable to reach ThingSpeak. Check your internet connection and try again.";
    }
  }
  return "Unable to load this channel. Please check that the Channel ID is correct and that the ThingSpeak channel is public.";
}

function toThingSpeakDate(date) {
  return date.toISOString().replace("T", " ").replace(/\.\d{3}Z$/, "");
}

function formatTemperature(value) {
  return new Intl.NumberFormat(undefined, {
    minimumFractionDigits: 1,
    maximumFractionDigits: 2,
  }).format(value);
}

function formatTimestamp(value) {
  const date = value instanceof Date ? value : new Date(value);
  if (Number.isNaN(date.getTime())) return "";
  return new Intl.DateTimeFormat(undefined, {
    dateStyle: "medium",
    timeStyle: "short",
  }).format(date);
}

function formatAxisTime(timestamp) {
  const date = new Date(timestamp);
  const options = state.chartSpan > 1000 * 60 * 60 * 48
    ? { month: "short", day: "numeric" }
    : { hour: "numeric", minute: "2-digit" };
  return new Intl.DateTimeFormat(undefined, options).format(date);
}

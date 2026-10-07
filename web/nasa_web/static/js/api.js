const base = location.pathname.replace(/[^/]*$/, "");

async function request(path, options) {
  const res = await fetch(base + path, options);
  if (!res.ok) {
    let detail = res.statusText;
    try { detail = (await res.json()).detail || detail; } catch (e) { /* not json */ }
    throw new Error(detail);
  }
  return res.json();
}

export const api = {
  status: () => request("api/status"),

  history: (ids, start, end, points = 600) =>
    request(`api/history?ids=${encodeURIComponent(ids.join(","))}&start=${Math.floor(start)}&end=${Math.floor(end)}&points=${points}`),

  valuesAt: (ids, timestamps) =>
    request(`api/values_at?ids=${encodeURIComponent(ids.join(","))}&ts=${timestamps.map(Math.floor).join(",")}`),

  command: (id, value) =>
    request(`api/entities/${encodeURIComponent(id)}/command`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ value }),
    }),
};

(function () {
  "use strict";

  // ---------- theme ----------
  var root = document.documentElement;
  var themeBtn = document.getElementById("theme-btn");
  function currentTheme() {
    var t = root.getAttribute("data-theme");
    if (t) return t;
    return window.matchMedia && window.matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light";
  }
  if (themeBtn) {
    themeBtn.addEventListener("click", function () {
      var next = currentTheme() === "dark" ? "light" : "dark";
      root.setAttribute("data-theme", next);
      try { localStorage.setItem("sigma-theme", next); } catch (e) {}
    });
  }

  // ---------- mobile navigation ----------
  var menuBtn = document.getElementById("menu-btn");
  var scrim = document.getElementById("scrim");
  function setNav(open) {
    document.body.classList.toggle("nav-open", open);
    if (menuBtn) menuBtn.setAttribute("aria-expanded", open ? "true" : "false");
    if (scrim) scrim.hidden = !open;
  }
  if (menuBtn) menuBtn.addEventListener("click", function () { setNav(!document.body.classList.contains("nav-open")); });
  if (scrim) scrim.addEventListener("click", function () { setNav(false); });
  var active = document.querySelector(".sidebar a.active");
  if (active && active.scrollIntoView) active.scrollIntoView({ block: "center" });

  // ---------- tables scroll on small screens ----------
  document.querySelectorAll(".doc table").forEach(function (t) {
    if (t.parentElement.classList.contains("table-wrap")) return;
    var w = document.createElement("div");
    w.className = "table-wrap";
    t.parentNode.insertBefore(w, t);
    w.appendChild(t);
  });

  // ---------- copy buttons ----------
  document.querySelectorAll(".code:not(.out)").forEach(function (block) {
    var btn = document.createElement("button");
    btn.className = "copy-btn";
    btn.type = "button";
    btn.textContent = "Copy";
    btn.addEventListener("click", function () {
      var text = block.querySelector("code").innerText;
      function done() {
        btn.textContent = "Copied";
        setTimeout(function () { btn.textContent = "Copy"; }, 1400);
      }
      if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(text).then(done, function () {});
      } else {
        var ta = document.createElement("textarea");
        ta.value = text;
        document.body.appendChild(ta);
        ta.select();
        try { document.execCommand("copy"); done(); } catch (e) {}
        document.body.removeChild(ta);
      }
    });
    block.appendChild(btn);
  });

  // ---------- "on this page" highlighting ----------
  var tocLinks = Array.prototype.slice.call(document.querySelectorAll(".toc a"));
  if (tocLinks.length && "IntersectionObserver" in window) {
    var byId = {};
    tocLinks.forEach(function (a) { byId[a.getAttribute("href").slice(1)] = a; });
    var visible = {};
    var obs = new IntersectionObserver(function (entries) {
      entries.forEach(function (e) { visible[e.target.id] = e.isIntersecting; });
      var heads = document.querySelectorAll(".doc h2[id], .doc h3[id]");
      var current = null;
      for (var i = 0; i < heads.length; i++) {
        if (visible[heads[i].id]) { current = heads[i].id; break; }
      }
      if (!current) return;
      tocLinks.forEach(function (a) { a.classList.toggle("active", a === byId[current]); });
    }, { rootMargin: "-60px 0px -60% 0px" });
    document.querySelectorAll(".doc h2[id], .doc h3[id]").forEach(function (h) { obs.observe(h); });
  }

  // ---------- search ----------
  var input = document.getElementById("search-input");
  var box = document.getElementById("search-results");
  if (!input || !box) return;
  var index = null, loading = false, results = [], selected = -1;

  function load(cb) {
    if (index) return cb();
    if (loading) return;
    loading = true;
    fetch("search.json").then(function (r) { return r.json(); }).then(function (data) {
      index = data.map(function (e) {
        return { e: e, h: e.h.toLowerCase(), p: e.p.toLowerCase(), t: e.t.toLowerCase() };
      });
      cb();
    }).catch(function () {
      loading = false;
      box.innerHTML = '<div class="empty">Search is unavailable offline.</div>';
      box.hidden = false;
    });
  }

  function esc(s) {
    return s.replace(/[&<>"]/g, function (c) { return { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]; });
  }

  function mark(text, words) {
    var out = esc(text);
    words.forEach(function (w) {
      if (!w) return;
      var re = new RegExp("(" + w.replace(/[.*+?^${}()|[\]\\]/g, "\\$&") + ")", "ig");
      out = out.replace(re, "<mark>$1</mark>");
    });
    return out;
  }

  function snippet(text, words) {
    var lower = text.toLowerCase(), at = -1;
    for (var i = 0; i < words.length && at < 0; i++) at = lower.indexOf(words[i]);
    if (at < 0) return text.slice(0, 140);
    var start = Math.max(0, at - 50);
    return (start > 0 ? "..." : "") + text.slice(start, start + 160);
  }

  function search(q) {
    var words = q.toLowerCase().split(/\s+/).filter(Boolean);
    if (!words.length) return [];
    var scored = [];
    index.forEach(function (x) {
      var score = 0;
      for (var i = 0; i < words.length; i++) {
        var w = words[i], s = 0;
        if (x.h === w) s += 30;
        else if (x.h.indexOf(w) === 0) s += 16;
        else if (x.h.indexOf(w) >= 0) s += 10;
        if (x.p.indexOf(w) >= 0) s += 4;
        var c = x.t.indexOf(w);
        if (c >= 0) {
          s += 3;
          var re = new RegExp("\\b" + w.replace(/[.*+?^${}()|[\]\\]/g, "\\$&") + "\\b");
          if (re.test(x.t)) s += 2;
        }
        if (!s) return;
        score += s;
      }
      scored.push({ x: x, score: score });
    });
    scored.sort(function (a, b) { return b.score - a.score; });
    return scored.slice(0, 12).map(function (r) { return r.x.e; });
  }

  function render() {
    var q = input.value.trim();
    if (!q) { close(); return; }
    var words = q.toLowerCase().split(/\s+/).filter(Boolean);
    results = search(q);
    selected = results.length ? 0 : -1;
    if (!results.length) {
      box.innerHTML = '<div class="empty">No results for "' + esc(q) + '"</div>';
    } else {
      box.innerHTML = results.map(function (r, i) {
        return '<a href="' + r.u + '" role="option"' + (i === 0 ? ' class="selected"' : "") + ">" +
          '<div class="r-path">' + esc(r.p) + "</div>" +
          '<div class="r-head">' + mark(r.h, words) + "</div>" +
          (r.t ? '<div class="r-text">' + mark(snippet(r.t, words), words) + "</div>" : "") +
          "</a>";
      }).join("");
    }
    box.hidden = false;
    input.setAttribute("aria-expanded", "true");
  }

  function close() {
    box.hidden = true;
    input.setAttribute("aria-expanded", "false");
  }

  function move(d) {
    var links = box.querySelectorAll("a");
    if (!links.length) return;
    selected = (selected + d + links.length) % links.length;
    links.forEach(function (a, i) { a.classList.toggle("selected", i === selected); });
    links[selected].scrollIntoView({ block: "nearest" });
  }

  input.addEventListener("input", function () { load(render); });
  input.addEventListener("focus", function () { load(function () { if (input.value.trim()) render(); }); });
  input.addEventListener("keydown", function (e) {
    if (e.key === "ArrowDown") { e.preventDefault(); move(1); }
    else if (e.key === "ArrowUp") { e.preventDefault(); move(-1); }
    else if (e.key === "Enter") {
      var links = box.querySelectorAll("a");
      if (selected >= 0 && links[selected]) { e.preventDefault(); window.location.href = links[selected].getAttribute("href"); close(); }
    } else if (e.key === "Escape") { close(); input.blur(); }
  });
  box.addEventListener("click", function (e) { if (e.target.closest("a")) close(); });
  document.addEventListener("click", function (e) { if (!e.target.closest(".search")) close(); });
  document.addEventListener("keydown", function (e) {
    var tag = (document.activeElement && document.activeElement.tagName) || "";
    if ((e.key === "/" || ((e.metaKey || e.ctrlKey) && e.key.toLowerCase() === "k")) &&
        tag !== "INPUT" && tag !== "TEXTAREA") {
      e.preventDefault();
      input.focus();
      input.select();
    }
  });
})();

/* =========================================================================
   main.js — starfield, hit counter, guestbook and all the glorious retro
   toys that hold the DualityRF homepage together.
   ========================================================================= */
(function () {
  "use strict";

  var $ = function (id) { return document.getElementById(id); };
  var startTime = Date.now();

  function setStatus(msg) { var el = $("statusMsg"); if (el) el.textContent = msg; }

  /* =====================================================================
     3D VIEWER — kick it off and report progress into the viewer overlay
     ===================================================================== */
  function startViewer() {
    var vfill = $("vprogressfill"), vpct = $("vprogresspct"), vbox = $("vprogress");
    RPViewer.init({
      onProgress: function (pct, loaded) {
        var p = Math.round(pct * 100);
        if (vfill) vfill.style.width = p + "%";
        if (vpct) vpct.textContent = p + "%  (" + (loaded / 1048576).toFixed(1) + " MB)";
      },
      onLoaded: function () {
        if (vbox) vbox.classList.add("done");
        setStatus("Hardware model loaded. Drag to orbit.");
      },
      onError: function () { setStatus("Could not load model."); }
    });
  }

  /* =====================================================================
     STARFIELD
     ===================================================================== */
  function starfield() {
    var cv = $("stars"), ctx = cv.getContext("2d");
    var stars = [], W, H, warp = 1, targetWarp = 1;
    function resize() { W = cv.width = innerWidth; H = cv.height = innerHeight; seed(); }
    function seed() {
      stars = [];
      var n = Math.min(340, Math.floor((W * H) / 6000));
      for (var i = 0; i < n; i++) stars.push({ x: (Math.random() - 0.5) * W, y: (Math.random() - 0.5) * H, z: Math.random() * W });
    }
    function tick() {
      warp += (targetWarp - warp) * 0.05;
      ctx.fillStyle = "rgba(5,6,15,0.35)"; ctx.fillRect(0, 0, W, H);
      ctx.save(); ctx.translate(W / 2, H / 2);
      for (var i = 0; i < stars.length; i++) {
        var s = stars[i];
        s.z -= 2.2 * warp;
        if (s.z <= 1) { s.z = W; s.x = (Math.random() - 0.5) * W; s.y = (Math.random() - 0.5) * H; }
        var k = 128 / s.z, x = s.x * k, y = s.y * k;
        var size = (1 - s.z / W) * 2.4;
        var hue = (s.x + s.y) % 360;
        ctx.fillStyle = warp > 1.5 ? "hsl(" + Math.abs(hue) + ",100%,70%)" : "rgba(200,220,255," + (1 - s.z / W) + ")";
        ctx.fillRect(x, y, size, size);
      }
      ctx.restore();
      requestAnimationFrame(tick);
    }
    addEventListener("resize", resize); resize(); tick();
    return { warp: function (v) { targetWarp = v; } };
  }

  /* =====================================================================
     HIT COUNTER (localStorage, with an odometer roll)
     ===================================================================== */
  function hitCounter() {
    var el = $("counter");
    var key = "drf_hits";
    var n = parseInt(localStorage.getItem(key) || "0", 10);
    if (!n) n = 133700 + Math.floor(Math.random() * 42); // seed a respectably large legacy count
    n += 1;
    localStorage.setItem(key, String(n));
    var from = Math.max(0, n - 30), cur = from;
    (function roll() {
      el.textContent = String(cur).padStart(8, "0");
      if (cur < n) { cur++; setTimeout(roll, 28); }
    })();
  }

  /* =====================================================================
     GUESTBOOK (localStorage)
     ===================================================================== */
  function guestbook() {
    var key = "drf_guestbook", list = $("gbList"), form = $("gbForm");
    var entries;
    try { entries = JSON.parse(localStorage.getItem(key) || "null"); } catch (e) { entries = null; }
    if (!entries) {
      entries = [
        { name: "Sysop_Dave", mood: ":-)", msg: "first!! epic site dude. added u to my webring", ts: 994204800000 },
        { name: "wardriver99", mood: "8)", msg: "that 3D pi is sick. how'd u do that with no flash??", ts: 1012345678000 },
        { name: "QRP_Queen", mood: "^_^", msg: "73 de VK3XYZ, love the waterfall. keep transmitting!", ts: 1104537600000 }
      ];
      save();
    }
    function save() { localStorage.setItem(key, JSON.stringify(entries.slice(-100))); }
    function fmt(ts) { var d = new Date(ts); return d.toLocaleDateString() + " " + d.toLocaleTimeString(); }

    function render() {
      list.innerHTML = "";
      entries.slice().reverse().forEach(function (e) {
        var wrap = document.createElement("div"); wrap.className = "gb-entry";
        var head = document.createElement("div"); head.className = "gb-entry__head";
        var b = document.createElement("b"); b.textContent = e.mood + " " + e.name;
        var dt = document.createElement("span"); dt.className = "gb-entry__date"; dt.textContent = "  ·  " + fmt(e.ts);
        head.appendChild(b); head.appendChild(dt);
        var msg = document.createElement("div"); msg.className = "gb-entry__msg"; msg.textContent = e.msg;
        wrap.appendChild(head); wrap.appendChild(msg); list.appendChild(wrap);
      });
    }
    render();

    form.addEventListener("submit", function (ev) {
      ev.preventDefault();
      var name = $("gbName").value.trim(), msg = $("gbMsg").value.trim();
      if (!name || !msg) return;
      entries.push({ name: name, mood: $("gbMood").value, msg: msg, ts: Date.now() });
      save(); render();
      form.reset();
      setStatus("Guestbook signed. You rule!");
      confettiRain();
    });
  }

  /* =====================================================================
     CLOCKS / UPTIME
     ===================================================================== */
  function clocks() {
    var clock = $("clock"), up = $("uptime"), today = $("today");
    if (today) today.textContent = new Date().toLocaleDateString();
    setInterval(function () {
      var d = new Date();
      if (clock) clock.textContent = d.toLocaleTimeString();
      var s = Math.floor((Date.now() - startTime) / 1000);
      if (up) up.textContent = "uptime " + (s < 60 ? s + "s" : Math.floor(s / 60) + "m " + (s % 60) + "s");
    }, 1000);
  }

  /* =====================================================================
     TABS (smooth scroll + scroll-spy)
     ===================================================================== */
  function tabs() {
    var links = Array.prototype.slice.call(document.querySelectorAll(".tab[href^='#']"));
    links.forEach(function (a) {
      a.addEventListener("click", function (e) {
        var id = a.getAttribute("href");
        if (id.length < 2) return;
        var sec = document.querySelector(id);
        if (sec) { e.preventDefault(); sec.scrollIntoView({ behavior: "smooth", block: "start" }); }
      });
    });
    var sections = ["home", "viewer", "features", "guestbook"].map($);
    if (!window.IntersectionObserver) return;
    var io = new IntersectionObserver(function (ents) {
      ents.forEach(function (en) {
        if (!en.isIntersecting) return;
        links.forEach(function (a) { a.classList.toggle("tab--on", a.getAttribute("href") === "#" + en.target.id); });
      });
    }, { rootMargin: "-40% 0px -55% 0px" });
    sections.forEach(function (s) { if (s) io.observe(s); });
  }

  /* =====================================================================
     3D TOOLBAR
     ===================================================================== */
  function toolbar() {
    $("btnRotate").addEventListener("click", function () {
      this.textContent = "Auto-Spin: " + (RPViewer.toggleRotate() ? "ON" : "OFF");
    });
    $("btnWire").addEventListener("click", function () {
      this.textContent = RPViewer.toggleWireframe() ? "Solid" : "Wireframe";
    });
    $("btnSkin").addEventListener("click", function () {
      this.textContent = "Skin: " + RPViewer.cycleSkin();
    });
    $("btnReset").addEventListener("click", function () { RPViewer.reset(); setStatus("View reset."); });
    $("btnShot").addEventListener("click", function () { RPViewer.snapshot(); setStatus("Snapshot saved to Downloads."); });
  }

  /* =====================================================================
     SIDEBAR: assistant, webring
     ===================================================================== */
  function assistant() {
    var say = $("assistantSay"), face = $("assistantFace");
    var tips = [
      "Psst — drag the Raspberry Pi to spin it!",
      "Did you know? DualityRF is 100% offline. No cloud!",
      "Tip: hit the Skin button to chrome-plate the board.",
      "Remember to sign the guestbook before you leave.",
      "Best viewed with the CRT filter ON. Very cozy.",
      "Try the Konami code. I won't tell anyone. Up Up Down Down...",
      "73! That's ham-speak for 'best regards'.",
      "Powered by three.js and pure nostalgia."
    ];
    var i = 0;
    function next() { say.textContent = tips[i % tips.length]; i++; }
    next();
    face.addEventListener("click", next);
    setInterval(next, 7000);
  }

  function webring() {
    var members = ["The RF Dungeon", "HamShack 2000", "SpectrumZone", "PacketRadio Palace",
                   "The Waterfall Cafe", "dBm Diner", "Antenna Alley", "IQ Underground"];
    var name = $("ringName"), i = 0;
    function show(n) { i = (n + members.length) % members.length; name.textContent = "» " + members[i] + " «"; }
    show(Math.floor(Math.random() * members.length));
    $("ringPrev").onclick = function () { show(i - 1); setStatus("Web-ring: previous site."); };
    $("ringNext").onclick = function () { show(i + 1); setStatus("Web-ring: next site."); };
    $("ringRand").onclick = function () { show(Math.floor(Math.random() * members.length)); setStatus("Web-ring: warping…"); };
  }

  /* =====================================================================
     WINDOW CHROME + FX
     ===================================================================== */
  function chrome(field) {
    var app = $("app");
    document.querySelectorAll(".win__btn").forEach(function (b) {
      b.addEventListener("click", function () {
        var fx = b.getAttribute("data-fx");
        if (fx === "min") app.classList.toggle("min");
        else if (fx === "max") app.classList.toggle("max");
        else {
          app.classList.add("shake");
          setTimeout(function () { app.classList.remove("shake"); }, 420);
          $("assistantSay").textContent = "You can't close me! We're having fun.";
          setStatus("Access denied: fun in progress.");
        }
      });
    });

    $("crtToggle").addEventListener("click", function () {
      var off = $("crt").classList.toggle("off");
      setStatus("CRT filter " + (off ? "OFF" : "ON") + ".");
    });

    var hyperOn = false;
    function hyperdrive() {
      hyperOn = !hyperOn;
      document.body.classList.toggle("hyper", hyperOn);
      field.warp(hyperOn ? 6 : 1);
      RPViewer.spin(hyperOn ? 26 : 2.2);
      setStatus(hyperOn ? "HYPERDRIVE ENGAGED" : "Hyperdrive disengaged.");
      if (hyperOn) confettiRain();
    }
    $("hyperBtn").addEventListener("click", hyperdrive);
    $("secretTab").addEventListener("click", function (e) {
      e.preventDefault();
      confettiRain();
      setStatus("You found the secret disk! +1 nostalgia.");
    });

    // Konami code
    var seq = [38, 38, 40, 40, 37, 39, 37, 39, 66, 65], pos = 0;
    addEventListener("keydown", function (e) {
      pos = (e.keyCode === seq[pos]) ? pos + 1 : (e.keyCode === seq[0] ? 1 : 0);
      if (pos === seq.length) { pos = 0; hyperdrive(); $("assistantSay").textContent = "CHEAT ACTIVATED! You're a true 90s kid."; }
    });
  }

  /* =====================================================================
     CONFETTI RAIN (retro symbols, no emoji)
     ===================================================================== */
  function confettiRain() {
    var set = ["★", "✦", "✧", "◆", "▓", "♦", "»"];
    var cols = ["#ff00cc", "#00ffff", "#39ff14", "#ffbf00", "#9d4edd"];
    for (var i = 0; i < 28; i++) {
      (function (n) {
        var s = document.createElement("div");
        s.textContent = set[n % set.length];
        s.style.cssText = "position:fixed;z-index:9500;top:-40px;left:" + (Math.random() * 100) +
          "vw;color:" + cols[n % cols.length] + ";text-shadow:0 0 6px currentColor;font-size:" +
          (14 + Math.random() * 20) + "px;pointer-events:none;transition:transform 2.4s linear,opacity 2.4s;";
        document.body.appendChild(s);
        requestAnimationFrame(function () {
          s.style.transform = "translateY(110vh) rotate(" + (Math.random() * 720 - 360) + "deg)";
          s.style.opacity = "0";
        });
        setTimeout(function () { s.remove(); }, 2500);
      })(i);
    }
  }

  /* =====================================================================
     GO
     ===================================================================== */
  document.addEventListener("DOMContentLoaded", function () {
    var field = starfield();
    hitCounter();
    guestbook();
    clocks();
    tabs();
    assistant();
    webring();
    chrome(field);
    if (window.THREE && window.RPViewer) { toolbar(); startViewer(); }
    else {
      var vbox = $("vprogress");
      if (vbox) vbox.innerHTML = '<div class="viewer__progress-label">WebGL/three.js unavailable</div>';
    }
    setStatus("Ready. Sign the guestbook!");
  });
})();

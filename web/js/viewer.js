/* =========================================================================
   viewer.js — the "3D LAB": a live WebGL STL viewer for Raspberry_Pi_3.STL.
   Vanilla three.js (r128 UMD global `THREE`). Exposes window.RPViewer.
   ========================================================================= */
(function () {
  "use strict";

  var MODEL_URL = "public/Raspberry_Pi_3.STL";

  // Retro "skins" cycled by the toolbar button.
  var SKINS = [
    { name: "PCB",       make: function () { return new THREE.MeshPhongMaterial({ color: 0x1c7a3e, specular: 0x225533, shininess: 30, flatShading: false }); } },
    { name: "GOLD",      make: function () { return new THREE.MeshPhongMaterial({ color: 0xffcc33, specular: 0xffffaa, shininess: 90 }); } },
    { name: "CHROME",    make: function () { return new THREE.MeshPhongMaterial({ color: 0xcfd6e6, specular: 0xffffff, shininess: 120 }); } },
    { name: "HOLOGRAM",  make: function () { return new THREE.MeshBasicMaterial({ color: 0x00ffff, wireframe: true }); } },
    { name: "X-RAY",     make: function () { return new THREE.MeshBasicMaterial({ color: 0x66ffcc, transparent: true, opacity: 0.35, blending: THREE.AdditiveBlending, depthWrite: false }); } }
  ];

  var scene, camera, renderer, controls, mesh, grid;
  var stage, hudFps, hudTris;
  var skinIndex = 0, wireframe = false, autoRotate = true;
  var frames = 0, fpsLast = performance.now();
  var homeState = null; // camera framing for "reset view"

  function make(cb) {
    stage = document.getElementById("stage");
    hudFps = document.getElementById("fps");
    hudTris = document.getElementById("tris");

    scene = new THREE.Scene();
    scene.fog = new THREE.FogExp2(0x050914, 0.0025);

    camera = new THREE.PerspectiveCamera(45, aspect(), 0.1, 5000);
    camera.position.set(120, 90, 160);

    renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true, preserveDrawingBuffer: true });
    renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 2));
    renderer.setSize(stage.clientWidth, stage.clientHeight);
    stage.appendChild(renderer.domElement);

    // Lights: soft sky/ground + a key light + a coloured rim for that neon vibe.
    scene.add(new THREE.HemisphereLight(0xbfd4ff, 0x101020, 0.9));
    var key = new THREE.DirectionalLight(0xffffff, 0.9);
    key.position.set(1, 2, 1.5);
    scene.add(key);
    var rim = new THREE.PointLight(0xff00cc, 0.6, 0);
    rim.position.set(-120, 40, -120);
    scene.add(rim);
    var rim2 = new THREE.PointLight(0x00ffff, 0.5, 0);
    rim2.position.set(120, -30, 120);
    scene.add(rim2);

    controls = new THREE.OrbitControls(camera, renderer.domElement);
    controls.enableDamping = true;
    controls.dampingFactor = 0.08;
    controls.autoRotate = autoRotate;
    controls.autoRotateSpeed = 2.2;

    loadModel(cb);
    window.addEventListener("resize", onResize);
    if (window.ResizeObserver) new ResizeObserver(onResize).observe(stage);
    animate();
  }

  function aspect() {
    var s = document.getElementById("stage");
    return s.clientWidth / Math.max(1, s.clientHeight);
  }

  function buildMesh(geometry, cb) {
    geometry.center();
    geometry.rotateX(-Math.PI / 2); // most Pi STLs are Z-up; lay it flat
    geometry.computeVertexNormals();
    geometry.computeBoundingBox();
    geometry.computeBoundingSphere();

    mesh = new THREE.Mesh(geometry, SKINS[skinIndex].make());
    scene.add(mesh);

    // Retro wire-floor under the board.
    var r = geometry.boundingSphere.radius;
    var span = Math.ceil((r * 2.6) / 20) * 20;
    grid = new THREE.GridHelper(span, span / 10, 0x00ffff, 0x223355);
    grid.position.y = geometry.boundingBox.min.y - 2;
    grid.material.transparent = true;
    grid.material.opacity = 0.5;
    scene.add(grid);

    frameObject(geometry.boundingSphere);
    if (hudTris) hudTris.textContent = (geometry.attributes.position.count / 3 | 0).toLocaleString() + " tris";
    var box = document.getElementById("vprogress");
    if (box) box.classList.add("done");
    if (cb && cb.onLoaded) cb.onLoaded();
  }

  function loadModel(cb) {
    // Opened straight from disk (file://): browsers block XHR, so skip the
    // network path and offer a file picker instead.
    if (location.protocol === "file:") { showPicker(cb, "Opened from disk (file://)."); return; }

    var loader = new THREE.STLLoader();
    loader.load(
      MODEL_URL,
      function (geometry) { buildMesh(geometry, cb); },
      function (ev) {
        if (cb && cb.onProgress) {
          var pct = ev.lengthComputable ? ev.loaded / ev.total : (ev.loaded % 5e6) / 5e6;
          cb.onProgress(Math.min(1, pct), ev.loaded);
        }
      },
      function () { showPicker(cb, "Auto-load blocked."); }
    );
  }

  // Fallback UI: a click-to-load prompt that reads the STL locally via
  // FileReader (works even from file://, no server needed).
  function showPicker(cb, reason) {
    var box = document.getElementById("vprogress");
    if (!box) return;
    box.classList.remove("done");
    box.innerHTML = "";

    var label = document.createElement("div");
    label.className = "viewer__progress-label";
    label.textContent = reason + " Click to load the model:";

    var input = document.createElement("input");
    input.type = "file";
    input.accept = ".stl,.STL";
    input.style.display = "none";

    var btn = document.createElement("button");
    btn.className = "btn btn--big";
    btn.textContent = "▶ Load Raspberry_Pi_3.STL";
    btn.onclick = function () { input.click(); };

    var hint = document.createElement("div");
    hint.className = "viewer__progress-pct";
    hint.style.fontSize = "11px";
    hint.textContent = "(tip: serve over HTTP to skip this)";

    input.onchange = function () {
      var f = input.files && input.files[0];
      if (!f) return;
      label.textContent = "Reading " + f.name + " …";
      var reader = new FileReader();
      reader.onprogress = function (e) {
        if (cb && cb.onProgress && e.lengthComputable) cb.onProgress(e.loaded / e.total, e.loaded);
      };
      reader.onload = function () {
        try { buildMesh(new THREE.STLLoader().parse(reader.result), cb); }
        catch (err) { label.textContent = "Could not parse that STL."; }
      };
      reader.readAsArrayBuffer(f);
    };

    box.appendChild(label);
    box.appendChild(btn);
    box.appendChild(hint);
    box.appendChild(input);
  }

  function frameObject(sphere) {
    var r = sphere.radius;
    var dist = (r / Math.sin((camera.fov * Math.PI / 180) / 2)) * 1.3;
    var dir = new THREE.Vector3(0.7, 0.5, 1).normalize();
    camera.position.copy(dir.multiplyScalar(dist));
    camera.near = dist / 100;
    camera.far = dist * 100;
    camera.updateProjectionMatrix();
    controls.target.set(0, 0, 0);
    controls.update();
    homeState = { pos: camera.position.clone(), tgt: controls.target.clone() };
  }

  function onResize() {
    if (!renderer) return;
    var w = stage.clientWidth, h = stage.clientHeight;
    camera.aspect = w / Math.max(1, h);
    camera.updateProjectionMatrix();
    renderer.setSize(w, h);
  }

  function animate() {
    requestAnimationFrame(animate);
    if (controls) controls.update();
    if (renderer) renderer.render(scene, camera);

    frames++;
    var now = performance.now();
    if (now - fpsLast >= 500) {
      var fps = Math.round((frames * 1000) / (now - fpsLast));
      if (hudFps) hudFps.textContent = fps + " FPS";
      frames = 0; fpsLast = now;
    }
  }

  // ---- public API (wired to toolbar buttons in main.js) ----
  window.RPViewer = {
    init: make,
    toggleRotate: function () {
      autoRotate = !autoRotate;
      if (controls) controls.autoRotate = autoRotate;
      return autoRotate;
    },
    toggleWireframe: function () {
      wireframe = !wireframe;
      if (mesh && mesh.material && "wireframe" in mesh.material) mesh.material.wireframe = wireframe;
      return wireframe;
    },
    cycleSkin: function () {
      skinIndex = (skinIndex + 1) % SKINS.length;
      if (mesh) {
        mesh.material.dispose();
        mesh.material = SKINS[skinIndex].make();
        if ("wireframe" in mesh.material) mesh.material.wireframe = wireframe;
      }
      return SKINS[skinIndex].name;
    },
    reset: function () {
      if (!homeState) return;
      camera.position.copy(homeState.pos);
      controls.target.copy(homeState.tgt);
      controls.update();
    },
    snapshot: function () {
      if (!renderer) return;
      renderer.render(scene, camera);
      var a = document.createElement("a");
      a.download = "dualityrf_pi3_" + Date.now() + ".png";
      a.href = renderer.domElement.toDataURL("image/png");
      a.click();
    },
    spin: function (times) { // easter-egg hyperspin
      if (controls) { controls.autoRotateSpeed = times; }
    }
  };
})();

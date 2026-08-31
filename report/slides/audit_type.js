/* Type-size audit for the deck: walks every slide AND every beat and reports the
   REAL rendered size of every visible run of text, in stage pixels (the stage is a
   fixed 1280x720 canvas, so a stage pixel is what the 720p recording sees).

   Two things it does that reading the markup cannot:
     (a) SVG <text> is measured through getScreenCTM(), so a font-size="16" inside a
         viewBox that renders narrower than its user units is reported at the size it
         actually paints, not at 16.  Most of the deck's small type is in scenes, and
         this is the only way to see it.
     (b) inherited and shorthand sizes are resolved by the engine, so nothing is
         missed because it had no font-size of its own.

   Rehearsal chrome (#foot, #notes, #help, #grid, #safe) is excluded: it is turned
   off before recording and is not in shot.

   Output goes into <pre id="zzTYPEOUT"> so --dump-dom can read it. */
(function () {
  var FLOOR = +(location.search.match(/floor=([0-9.]+)/) || [0, 19])[1];
  var out = [], seen = {};

  /* Design px = the size on the 1280x720 canvas, which is what the recording sees.
     For ordinary HTML that IS getComputedStyle's font-size: the stage's transform
     scales the whole canvas and does not change computed style.  For SVG <text> the
     computed size is in USER units, so it must be multiplied by the viewBox's
     user->px factor -- and that factor has to be taken RELATIVE to the stage, since
     getScreenCTM() also carries the stage's own scale-to-window.  Mixing the two
     (dividing HTML by the stage scale as well) inflates every HTML run by 1/scale;
     that is why a first run reported slide 13's floor as 25.5px when its CSS says 22. */
  function userScale(el) {
    if (!el.ownerSVGElement || !el.getScreenCTM) return 1;
    var m = el.getScreenCTM();
    var stage = document.getElementById('stage');
    if (!m || !stage) return 1;
    var sw = stage.getBoundingClientRect().width / 1280;   // stage -> window
    var s = Math.sqrt(Math.abs(m.a * m.d - m.b * m.c));     // user  -> window
    return sw ? s / sw : s;                                 // user  -> stage px
  }

  function ownText(el) {
    var s = '';
    for (var i = 0; i < el.childNodes.length; i++) {
      if (el.childNodes[i].nodeType === 3) s += el.childNodes[i].nodeValue;
    }
    return s.replace(/\s+/g, ' ').trim();
  }

  function measure(tag) {
    var slide = document.querySelector('.slide.active');
    if (!slide) return;
    slide.querySelectorAll('*').forEach(function (el) {
      var t = ownText(el);
      if (!t) return;
      var cs = getComputedStyle(el);
      if (cs.visibility === 'hidden' || cs.display === 'none' || +cs.opacity === 0) return;
      var b = el.getBoundingClientRect();
      if (b.width === 0 || b.height === 0) return;
      var px = parseFloat(cs.fontSize) * userScale(el);
      if (px >= FLOOR - 0.01) return;
      var key = tag.split(' ')[0] + '|' + el.tagName + '|' + t.slice(0, 40) + '|' + px.toFixed(1);
      if (seen[key]) return;
      seen[key] = 1;
      out.push(px.toFixed(1).padStart(5) + 'px  ' + tag.split(' ')[0].padEnd(4) +
               '  ' + el.tagName.toLowerCase() +
               (typeof el.className === 'string' && el.className ? '.' + el.className.split(' ').filter(Boolean).slice(0, 2).join('.') : '') +
               '  "' + t.slice(0, 64) + '"');
    });
  }

  window.addEventListener('load', function () {
    setTimeout(function () {
      try {
        var slides = document.querySelectorAll('.slide');
        var subs = [];
        slides.forEach(function (s) { subs.push(+(s.dataset.sub || 0)); });
        location.hash = '#1';
        for (var i = 0; i < slides.length; i++) {
          for (var b = 0; b <= subs[i]; b++) {
            var idx = document.querySelector('.slide.active');
            var n = Array.prototype.indexOf.call(slides, idx) + 1;
            measure('s' + n + ' beat' + (b + 1));
            document.dispatchEvent(new KeyboardEvent('keydown', { key: 'ArrowRight' }));
          }
        }
        out.sort(function (a, b) { return parseFloat(a) - parseFloat(b); });
        var pre = document.createElement('pre');
        pre.id = 'zzTYPEOUT';
        pre.textContent = 'FLOOR ' + FLOOR + 'px — ' + out.length + ' run(s) below it\n' +
                          (out.length ? out.join('\n') : 'CLEAN');
        document.body.appendChild(pre);
      } catch (e) {
        var p2 = document.createElement('pre');
        p2.id = 'zzTYPEOUT';
        p2.textContent = 'ERROR ' + e.message + ' | partial ' + out.length + '\n' + out.join('\n');
        document.body.appendChild(p2);
      }
    }, 2600);
  });
})();

/* Overflow audit for the deck: walks every slide AND every beat, measuring real
   geometry rather than guessing at font metrics.  Flags
     (a) any element painting outside the .slide content box,
     (b) any element wider/taller than the card-like ancestor that frames it,
     (c) any SVG <text> wider than the <rect> drawn behind it in the same <g>.
   Results are written into a sentinel element so --dump-dom can read them. */
(function () {
  function box(el) { return el.getBoundingClientRect(); }
  function isFramed(el) {
    var s = getComputedStyle(el);
    return (s.borderTopWidth !== '0px' || s.backgroundColor !== 'rgba(0, 0, 0, 0)') &&
           el.classList.length > 0;
  }
  var out = [];
  function measure(tag) {
    var slide = document.querySelector('.slide.active');
    if (!slide) return;
    var sb = box(slide), cs = getComputedStyle(slide);
    var padB = sb.top + slide.clientHeight - parseFloat(cs.paddingBottom);
    var padR = sb.left + slide.clientWidth - parseFloat(cs.paddingRight);

    // (a) out of the slide
    slide.querySelectorAll('*').forEach(function (el) {
      if (!el.offsetParent && el.tagName !== 'text') return;
      var b = box(el);
      if (b.width === 0 || b.height === 0) return;
      if (b.bottom > padB + 1.5) out.push(tag + ' OUT-BOTTOM ' + desc(el) + ' by ' + (b.bottom - padB).toFixed(0) + 'px');
      if (b.right > padR + 1.5) out.push(tag + ' OUT-RIGHT  ' + desc(el) + ' by ' + (b.right - padR).toFixed(0) + 'px');
    });

    // (b) child escaping its framed ancestor
    slide.querySelectorAll('.card,.wparty,.wchain,.tx,.dcol,.dspend,.wobj,.chip').forEach(function (p) {
      var pb = box(p), pcs = getComputedStyle(p);
      if (pcs.overflow === 'hidden') return;
      p.querySelectorAll('*').forEach(function (el) {
        var b = box(el);
        if (b.width === 0 || b.height === 0) return;
        if (b.bottom > pb.bottom + 1.5) out.push(tag + ' ESCAPES ' + desc(p) + ' <- ' + desc(el) + ' bottom by ' + (b.bottom - pb.bottom).toFixed(0) + 'px');
        if (b.right > pb.right + 1.5) out.push(tag + ' ESCAPES ' + desc(p) + ' <- ' + desc(el) + ' right by ' + (b.right - pb.right).toFixed(0) + 'px');
      });
    });

    // (c) SVG text wider than the rect behind it
    slide.querySelectorAll('svg g').forEach(function (g) {
      var rects = g.querySelectorAll(':scope > rect');
      if (!rects.length) return;
      g.querySelectorAll(':scope > text').forEach(function (t) {
        var tb = box(t), best = null, bestArea = Infinity;
        rects.forEach(function (r) {
          var rb = box(r);
          // the rect that vertically contains this text line
          if (tb.top >= rb.top - 6 && tb.bottom <= rb.bottom + 6) {
            var a = rb.width * rb.height;
            if (a < bestArea) { bestArea = a; best = rb; }
          }
        });
        if (!best) return;
        if (tb.right > best.right - 2 || tb.left < best.left + 2) {
          out.push(tag + ' SVG-TEXT overruns its rect: "' + t.textContent.slice(0, 38) +
                   '" text[' + tb.left.toFixed(0) + '..' + tb.right.toFixed(0) +
                   '] rect[' + best.left.toFixed(0) + '..' + best.right.toFixed(0) + ']');
        }
      });
    });
  }
  function desc(el) {
    return el.tagName.toLowerCase() + (el.className && el.className.baseVal === undefined && typeof el.className === 'string'
      ? '.' + el.className.split(' ').filter(Boolean).slice(0, 2).join('.') : '') +
      ' "' + (el.textContent || '').replace(/\s+/g,' ').trim().slice(0, 30) + '"';
  }

  window.addEventListener('load', function () {
    setTimeout(function () {
     try {
      var slides = document.querySelectorAll('.slide');
      var total = slides.length;
      var subs = [];
      slides.forEach(function (s) { subs.push(+(s.dataset.sub || 0)); });
      // walk from slide 1 beat 1 forward through every beat of every slide
      location.hash = '#1';
      for (var i = 0; i < total; i++) {
        for (var b = 0; b <= subs[i]; b++) {
          var idx = document.querySelector('.slide.active');
          var n = Array.prototype.indexOf.call(slides, idx) + 1;
          measure('s' + n + ' beat' + (b + 1) + '/' + (subs[i] + 1));
          document.dispatchEvent(new KeyboardEvent('keydown', { key: 'ArrowRight' }));
        }
      }
      var pre = document.createElement('pre');
      pre.id = 'zzAUDITOUT';
      pre.textContent = out.length ? out.join('\n') : 'CLEAN';
      document.body.appendChild(pre);
     } catch(e){ var p2=document.createElement('pre'); p2.id='zzAUDITOUT'; p2.textContent='ERROR '+e.message+' | partial '+out.length; document.body.appendChild(p2); }
    }, 2600);
  });
})();

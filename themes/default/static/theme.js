(function () {
  var currentStep = 1;

  function applyMask(el, pattern) {
    el.addEventListener('input', function () {
      var digits = el.value.replace(/\D/g, '');
      var result = '';
      var di = 0;
      for (var i = 0; i < pattern.length && di < digits.length; i++) {
        if (pattern[i] === '0') {
          result += digits[di++];
        } else {
          result += pattern[i];
          if (digits[di] === pattern[i]) { di++; }
        }
      }
      el.value = result;
    });
  }

  function initMasks() {
    var masks = [
      { id: 'cep',         pattern: '00000-000' },
      { id: 'phone',       pattern: '(00) 00000-0000' },
      { id: 'card_number', pattern: '0000 0000 0000 0000' },
      { id: 'card_expiry', pattern: '00/00' },
    ];
    masks.forEach(function (m) {
      var el = document.getElementById(m.id);
      if (el) { applyMask(el, m.pattern); }
    });
  }

  function validateStep(n) {
    var section = document.querySelector('[data-step="' + n + '"]');
    if (!section) { return true; }
    var inputs = section.querySelectorAll('input[required], select[required]');
    var ok = true;
    inputs.forEach(function (inp) {
      inp.classList.remove('co-input--error');
      if (!inp.value.trim()) {
        inp.classList.add('co-input--error');
        inp.focus();
        ok = false;
      }
    });
    return ok;
  }

  function summaryText(n) {
    if (n === 1) {
      var name  = (document.getElementById('name')  || {}).value || '';
      var street = (document.getElementById('street') || {}).value || '';
      var num    = (document.getElementById('number') || {}).value || '';
      var city   = (document.getElementById('city')  || {}).value || '';
      return [name, street && (street + (num ? ', ' + num : '')), city].filter(Boolean).join(' · ');
    }
    if (n === 2) {
      var checked = document.querySelector('input[name="shipping_method"]:checked');
      if (!checked) { return ''; }
      var label = checked.closest('.co-shipping-opt');
      var sname = label ? (label.querySelector('.co-shipping-name') || {}).textContent : checked.value;
      return sname || checked.value;
    }
    return '';
  }

  function goToStep(n) {
    var steps     = document.querySelectorAll('.co-step');
    var indicators = document.querySelectorAll('[data-indicator]');
    var lines     = document.querySelectorAll('.co-progress-line');

    steps.forEach(function (s) {
      var sn = parseInt(s.dataset.step, 10);
      s.classList.remove('co-step--active', 'co-step--done', 'co-step--locked');

      var editBtn  = s.querySelector('.co-btn-edit');
      var summaryEl = s.querySelector('.co-step-summary');

      if (sn < n) {
        s.classList.add('co-step--done');
        if (editBtn)   { editBtn.hidden = false; }
        if (summaryEl) { summaryEl.textContent = summaryText(sn); }
      } else if (sn === n) {
        s.classList.add('co-step--active');
        if (editBtn) { editBtn.hidden = true; }
        s.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
      } else {
        s.classList.add('co-step--locked');
        if (editBtn) { editBtn.hidden = true; }
      }
    });

    indicators.forEach(function (ind) {
      var in_ = parseInt(ind.dataset.indicator, 10);
      ind.classList.remove('co-progress-step--active', 'co-progress-step--done');
      if (in_ < n)       { ind.classList.add('co-progress-step--done'); }
      else if (in_ === n) { ind.classList.add('co-progress-step--active'); }
    });

    lines.forEach(function (line, i) {
      line.classList.toggle('co-progress-line--done', i < n - 1);
    });

    currentStep = n;
  }

  function initStepNav() {
    document.addEventListener('click', function (e) {
      var nextBtn = e.target.closest('[data-goto]');
      if (!nextBtn) { return; }

      var target = parseInt(nextBtn.dataset.goto, 10);

      if (target > currentStep) {
        if (!validateStep(currentStep)) { return; }
      }

      goToStep(target);
    });
  }

  function initPayTabs() {
    var tabs = document.querySelectorAll('.co-pay-tab');
    if (!tabs.length) { return; }
    tabs.forEach(function (tab) {
      tab.addEventListener('click', function () {
        var target = tab.dataset.tab;
        tabs.forEach(function (t) {
          t.classList.remove('co-pay-tab--active');
          t.setAttribute('aria-selected', 'false');
        });
        document.querySelectorAll('.co-tab-panel').forEach(function (p) {
          p.classList.add('co-tab-panel--hidden');
        });
        tab.classList.add('co-pay-tab--active');
        tab.setAttribute('aria-selected', 'true');
        var panel = document.getElementById('co-panel-' + target);
        if (panel) { panel.classList.remove('co-tab-panel--hidden'); }
      });
    });
  }

  function initCepLookup() {
    var cep = document.getElementById('cep');
    if (!cep) { return; }
    cep.addEventListener('blur', function () {
      var digits = cep.value.replace(/\D/g, '');
      if (digits.length !== 8) { return; }
      fetch('https://viacep.com.br/ws/' + digits + '/json/')
        .then(function (r) { return r.ok ? r.json() : null; })
        .then(function (data) {
          if (!data || data.erro) { return; }
          var set = function (id, val) {
            var el = document.getElementById(id);
            if (el && val) { el.value = val; }
          };
          set('street',       data.logradouro);
          set('neighborhood', data.bairro);
          set('city',         data.localidade);
          set('state',        data.uf);
          var num = document.getElementById('number');
          if (num) { num.focus(); }
        })
        .catch(function () {});
    });
  }

  function initCartFetch() {
    fetch('/api/cart', { credentials: 'include' })
      .then(function (r) { return r.ok ? r.json() : null; })
      .then(function (data) {
        if (!data || !data.total_display) { return; }
        document.querySelectorAll('[data-cart-total]').forEach(function (el) {
          el.textContent = '🔒 Finalizar compra — ' + data.total_display;
        });
      })
      .catch(function () {});
  }

  document.addEventListener('DOMContentLoaded', function () {
    initMasks();
    initStepNav();
    initPayTabs();
    initCepLookup();
    initCartFetch();
    goToStep(1);
  });
}());

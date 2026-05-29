(function () {
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
      { id: 'cpf',         pattern: '000.000.000-00' },
      { id: 'phone',       pattern: '(00) 00000-0000' },
      { id: 'card_number', pattern: '0000 0000 0000 0000' },
      { id: 'card_expiry', pattern: '00/00' },
    ];
    masks.forEach(function (m) {
      var el = document.getElementById(m.id);
      if (el) { applyMask(el, m.pattern); }
    });
  }

  function initTabs() {
    var tabs = document.querySelectorAll('.oc-pay-tab');
    if (!tabs.length) { return; }
    tabs.forEach(function (tab) {
      tab.addEventListener('click', function () {
        var target = tab.dataset.tab;
        tabs.forEach(function (t) {
          t.classList.remove('oc-pay-tab--active');
          t.setAttribute('aria-selected', 'false');
        });
        document.querySelectorAll('.oc-tab-panel').forEach(function (p) {
          p.classList.add('oc-tab-panel--hidden');
        });
        tab.classList.add('oc-pay-tab--active');
        tab.setAttribute('aria-selected', 'true');
        var panel = document.getElementById('panel-' + target);
        if (panel) { panel.classList.remove('oc-tab-panel--hidden'); }
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
          set('street', data.logradouro);
          set('neighborhood', data.bairro);
          set('city', data.localidade);
          set('state', data.uf);
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

  function initPixCopy() {
    var btn = document.querySelector('.oc-pix-copy');
    if (!btn) { return; }
    btn.addEventListener('click', function () {
      var code = btn.dataset.pixCode || '';
      if (!code) {
        btn.textContent = 'Código indisponível';
        return;
      }
      navigator.clipboard.writeText(code)
        .then(function () {
          var orig = btn.textContent;
          btn.textContent = '✓ Copiado!';
          setTimeout(function () { btn.textContent = orig; }, 2000);
        })
        .catch(function () {});
    });
  }

  document.addEventListener('DOMContentLoaded', function () {
    initMasks();
    initTabs();
    initCepLookup();
    initCartFetch();
    initPixCopy();
  });
}());

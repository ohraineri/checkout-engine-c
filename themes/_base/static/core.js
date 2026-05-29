let i18n = {};

const MASKS = {
  zipcode(v) {
    const d = v.replace(/\D/g, '').slice(0, 8);
    return d.length > 5 ? `${d.slice(0, 5)}-${d.slice(5)}` : d;
  },
  card(v) {
    const d = v.replace(/\D/g, '').slice(0, 16);
    return d.replace(/(.{4})(?=.)/g, '$1 ');
  },
  expiry(v) {
    const d = v.replace(/\D/g, '').slice(0, 4);
    return d.length > 2 ? `${d.slice(0, 2)}/${d.slice(2)}` : d;
  },
  cvv(v) {
    return v.replace(/\D/g, '').slice(0, 4);
  },
};

function applyMask(el, maskFn) {
  const raw = el.value;
  const start = el.selectionStart;
  const digitsBeforeCursor = raw.slice(0, start).replace(/\D/g, '').length;
  const formatted = maskFn(raw);
  el.value = formatted;
  let count = 0;
  let newPos = formatted.length;
  for (let i = 0; i < formatted.length; i++) {
    if (/\d/.test(formatted[i])) count++;
    if (count === digitsBeforeCursor) {
      newPos = i + 1;
      break;
    }
  }
  el.setSelectionRange(newPos, newPos);
}

function setupMasks() {
  document.addEventListener('input', e => {
    const fg = e.target.closest('[data-mask]');
    if (!fg) return;
    const fn = MASKS[fg.dataset.mask];
    if (fn) applyMask(e.target, fn);
  });
}

function setupFloatingLabels() {
  const markFilled = el => {
    const fg = el.closest('[data-field]');
    if (fg && el.value) fg.classList.add('filled');
  };
  const clearFilled = el => {
    const fg = el.closest('[data-field]');
    if (fg && !el.value) fg.classList.remove('filled');
  };
  document.querySelectorAll('[data-field] input, [data-field] select').forEach(el => {
    markFilled(el);
    el.addEventListener('input', () => markFilled(el));
    el.addEventListener('change', () => markFilled(el));
    el.addEventListener('blur', () => clearFilled(el));
  });
}

const VALIDATORS = {
  required(v) {
    return v.trim() ? null : i18n['validation.required'];
  },
  email(v) {
    return /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(v.trim()) ? null : i18n['validation.email'];
  },
  zipcode(v) {
    return v.replace(/\D/g, '').length === 8 ? null : i18n['validation.zipcode'];
  },
  'card-number'(v) {
    const digits = v.replace(/\D/g, '');
    if (digits.length < 13 || digits.length > 19) return i18n['validation.card-number'];
    let sum = 0;
    let alt = false;
    for (let i = digits.length - 1; i >= 0; i--) {
      let n = parseInt(digits[i], 10);
      if (alt) { n *= 2; if (n > 9) n -= 9; }
      sum += n;
      alt = !alt;
    }
    return sum % 10 === 0 ? null : i18n['validation.card-number'];
  },
  expiry(v) {
    const d = v.replace(/\D/g, '');
    if (d.length < 4) return i18n['validation.expiry'];
    const month = parseInt(d.slice(0, 2), 10);
    const year = parseInt(`20${d.slice(2, 4)}`, 10);
    if (month < 1 || month > 12) return i18n['validation.expiry'];
    const now = new Date();
    const exp = new Date(year, month - 1, 1);
    const cur = new Date(now.getFullYear(), now.getMonth(), 1);
    return exp >= cur ? null : i18n['validation.expiry'];
  },
  cvv(v) {
    return v.replace(/\D/g, '').length >= 3 ? null : i18n['validation.cvv'];
  },
};

function validateField(fg) {
  const rule = fg.dataset.validate;
  if (!rule) return true;
  const input = fg.querySelector('input, select');
  if (!input) return true;
  const validator = VALIDATORS[rule];
  if (!validator) return true;
  const error = validator(input.value);
  const errorEl = fg.querySelector('.field-error');
  if (error) {
    fg.classList.add('field-group--error');
    if (errorEl) errorEl.textContent = error;
    return false;
  }
  fg.classList.remove('field-group--error');
  if (errorEl) errorEl.textContent = '';
  return true;
}

function validateAll() {
  return [...document.querySelectorAll('[data-validate]')].every(validateField);
}

function setupValidation() {
  document.addEventListener('blur', e => {
    const fg = e.target.closest('[data-validate]');
    if (fg) validateField(fg);
  }, true);
}

const BRAND_PREFIXES = [
  ['visa',      v => v[0] === '4'],
  ['mastercard', v => {
    const n2 = parseInt(v.slice(0, 2), 10);
    const n4 = parseInt(v.slice(0, 4), 10);
    return (n2 >= 51 && n2 <= 55) || (n4 >= 2221 && n4 <= 2720);
  }],
  ['amex',       v => v.startsWith('34') || v.startsWith('37')],
  ['elo',        v => ['636368','438935','504175','451416','636297'].some(p => v.startsWith(p))],
  ['hipercard',  v => v.startsWith('606282')],
];

function detectBrand(value) {
  const v = value.replace(/\D/g, '');
  if (!v) return null;
  for (const [name, test] of BRAND_PREFIXES) {
    if (test(v)) return name;
  }
  return null;
}

function setupBrandDetection() {
  document.addEventListener('input', e => {
    const fg = e.target.closest('[data-field="card-number"]');
    if (!fg) return;
    const brand = detectBrand(e.target.value);
    if (brand) {
      fg.dataset.brand = brand;
    } else {
      delete fg.dataset.brand;
    }
  });
}

async function fetchAddress(cep) {
  const res = await fetch(`https://viacep.com.br/ws/${cep}/json/`);
  if (!res.ok) throw new Error(i18n['validation.cep-not-found']);
  const data = await res.json();
  if (data.erro) throw new Error(i18n['validation.cep-not-found']);
  return {
    street: data.logradouro,
    neighborhood: data.bairro,
    city: data.localidade,
    state: data.uf,
  };
}

function applyAddress(fg, data) {
  const fills = JSON.parse(fg.dataset.fills || '{}');
  for (const [key, selector] of Object.entries(fills)) {
    const el = document.querySelector(selector);
    if (el && data[key]) {
      el.value = data[key];
      el.dispatchEvent(new Event('input', { bubbles: true }));
    }
  }
}

function setupViaCep() {
  document.addEventListener('input', async e => {
    const fg = e.target.closest('[data-mask="zipcode"][data-fills]');
    if (!fg) return;
    const digits = e.target.value.replace(/\D/g, '');
    if (digits.length !== 8) return;
    fg.classList.add('field-group--loading');
    fg.classList.remove('field-group--error');
    try {
      const data = await fetchAddress(digits);
      applyAddress(fg, data);
      const nextInput = document.querySelector('[data-field="number"] input');
      if (nextInput) nextInput.focus();
    } catch {
      fg.classList.add('field-group--error');
      const errorEl = fg.querySelector('.field-error');
      if (errorEl) errorEl.textContent = i18n['validation.cep-not-found'];
    } finally {
      fg.classList.remove('field-group--loading');
    }
  });
}

function setupPaymentTabs() {
  document.addEventListener('click', e => {
    const tab = e.target.closest('[role="tablist"] [data-tab]');
    if (!tab) return;
    const tablist = tab.closest('[role="tablist"]');
    tablist.querySelectorAll('[role="tab"]').forEach(t => t.setAttribute('aria-selected', 'false'));
    tab.setAttribute('aria-selected', 'true');
    document.querySelectorAll('[data-panel]').forEach(p => { p.hidden = true; });
    const panel = document.querySelector(`[data-panel="${tab.dataset.tab}"]`);
    if (panel) panel.hidden = false;
  });
}

async function hydrateCart() {
  const removeSkeletons = () => {
    document.querySelectorAll('[data-cart]').forEach(el => el.classList.remove('skeleton'));
  };
  const fallback = setTimeout(removeSkeletons, 3000);
  try {
    const res = await fetch('/api/cart', { credentials: 'include' });
    if (!res.ok) { clearTimeout(fallback); removeSkeletons(); return; }
    const data = await res.json();
    document.querySelectorAll('[data-cart]').forEach(el => {
      const key = el.dataset.cart;
      if (data[key] !== undefined) {
        el.textContent = data[key];
        el.classList.remove('skeleton');
      }
    });
    clearTimeout(fallback);
  } catch {
    clearTimeout(fallback);
    removeSkeletons();
  }
}

function collectFormData() {
  const data = {};
  document.querySelectorAll('[data-field]').forEach(fg => {
    const input = fg.querySelector('input, select');
    if (input) data[fg.dataset.field] = input.value;
  });
  return data;
}

function setupSubmit() {
  document.addEventListener('click', e => {
    const btn = e.target.closest('[data-action="submit"]');
    if (!btn) return;
    if (!validateAll()) {
      const firstError = document.querySelector('[data-validate].field-group--error input, [data-validate].field-group--error select');
      if (firstError) firstError.focus();
      return;
    }
    btn.classList.add('loading');
    btn.disabled = true;
    document.dispatchEvent(new CustomEvent('checkout:submit', {
      detail: { formData: collectFormData() },
    }));
  });
}

function init() {
  const el = document.getElementById('checkout-i18n');
  if (el) {
    try { i18n = JSON.parse(el.textContent); } catch {}
  }
  setupMasks();
  setupFloatingLabels();
  setupValidation();
  setupBrandDetection();
  setupViaCep();
  setupPaymentTabs();
  hydrateCart();
  setupSubmit();
}

document.addEventListener('DOMContentLoaded', init);

window.CheckoutCore = { validateField, validateAll, detectBrand, fetchAddress };

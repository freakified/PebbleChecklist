let items = [];
let dragState = null;

// limits shared with the watchapp (MAX_CHECKLIST_ITEMS, MAX_NAME_LENGTH - 1
// and LIST_JSON_MAX_BYTES in the C code)
const MAX_ITEMS = 52;
const MAX_NAME_BYTES = 89;
const LIST_JSON_MAX_BYTES = 4000;

function getQueryParam(variable, defaultValue) {
  const query = location.search.substring(1);
  const vars = query.split("&");
  for (let i = 0; i < vars.length; i++) {
    const pair = vars[i].split("=");
    if (pair[0] === variable) return decodeURIComponent(pair[1]);
  }
  return defaultValue || false;
}

function parseCurrentState() {
  const state = window.CURRENT_STATE || getQueryParam("current_state", "[]");
  try {
    if (typeof state === 'string') {
      items = JSON.parse(state);
    } else {
      items = state;
    }
  }
  catch (e) { items = []; }
}

// settings bitfield flags (shared with the watchapp)
const SETTINGS_FLAG_SHOW_VOICE = 1;
const SETTINGS_FLAG_MOVE_CHECKED = 2;
const SETTINGS_FLAG_WRAP_AROUND = 4;
const SETTINGS_FLAG_LARGER_FONT = 8;
const SETTINGS_DEFAULT = SETTINGS_FLAG_SHOW_VOICE | SETTINGS_FLAG_WRAP_AROUND;

function parseCurrentSettings() {
  let bitfield = window.CURRENT_SETTINGS;
  if (typeof bitfield !== 'number') {
    bitfield = parseInt(getQueryParam("current_settings", ""), 10);
    if (isNaN(bitfield)) bitfield = SETTINGS_DEFAULT;
  }
  document.getElementById("setting_show_voice").checked = !!(bitfield & SETTINGS_FLAG_SHOW_VOICE);
  document.getElementById("setting_move_checked").checked = !!(bitfield & SETTINGS_FLAG_MOVE_CHECKED);
  document.getElementById("setting_wrap_around").checked = !!(bitfield & SETTINGS_FLAG_WRAP_AROUND);
  document.getElementById("setting_larger_font").checked = !!(bitfield & SETTINGS_FLAG_LARGER_FONT);
}

function getSettingsBitfield() {
  let bitfield = 0;
  if (document.getElementById("setting_show_voice").checked) bitfield |= SETTINGS_FLAG_SHOW_VOICE;
  if (document.getElementById("setting_move_checked").checked) bitfield |= SETTINGS_FLAG_MOVE_CHECKED;
  if (document.getElementById("setting_wrap_around").checked) bitfield |= SETTINGS_FLAG_WRAP_AROUND;
  if (document.getElementById("setting_larger_font").checked) bitfield |= SETTINGS_FLAG_LARGER_FONT;
  return bitfield;
}

function utf8Length(text) {
  return unescape(encodeURIComponent(text)).length;
}

// Shortens a name to what the watch can store, without splitting a character
function truncateName(text) {
  const chars = Array.from(text);
  while (utf8Length(chars.join("")) > MAX_NAME_BYTES) chars.pop();
  return chars.join("");
}

// The list exactly as it's sent to the watch
function getListJson() {
  return JSON.stringify(items.map(function (item) {
    return { n: truncateName(item.n.trim()), c: item.c ? 1 : 0 };
  }));
}

// Shows how close the list is to the watch's limits, and blocks adding or
// saving past them. An optional notice (e.g. from an import) is shown instead
// of the usual status.
function updateLimitStatus(notice) {
  const status = document.getElementById("list_status");
  const input = document.getElementById("new_item_input");
  const isFull = items.length >= MAX_ITEMS;
  const isTooLarge = items.length > MAX_ITEMS ||
    utf8Length(getListJson()) > LIST_JSON_MAX_BYTES;

  input.disabled = isFull;
  input.placeholder = isFull ? "List is full" : "Add new item...";
  document.getElementById("add_btn").disabled = isFull || !input.value.trim();
  document.getElementById("save_btn").disabled = isTooLarge;

  status.classList.toggle("error", isTooLarge);
  if (isTooLarge) {
    status.textContent = "This list is too long to send to your watch. Remove some items or shorten their names to save.";
  } else if (notice) {
    status.textContent = notice;
  } else if (isFull) {
    status.textContent = "List is full (" + MAX_ITEMS + " items).";
  } else if (items.length >= MAX_ITEMS - 5) {
    const left = MAX_ITEMS - items.length;
    status.textContent = "Room for " + left + " more item" + (left === 1 ? "" : "s") + ".";
  } else {
    status.textContent = "";
  }
}

// Warns if the watch couldn't fit its whole list into the message
function showTruncatedWarning() {
  const total = window.TOTAL_ITEMS;
  if (typeof total !== "number" || total <= items.length) return;
  const warning = document.getElementById("truncated_warning");
  warning.textContent = "Only " + items.length + " of the " + total +
    " items on your watch could be loaded. Saving will remove the rest.";
  warning.style.display = "block";
}

function escapeHtml(text) {
  const div = document.createElement("div");
  div.textContent = text;
  return div.innerHTML.replace(/"/g, '&quot;');
}

function renderItems() {
  const container = document.getElementById("items_list");
  container.innerHTML = "";
  items.forEach(function (item, index) {
    const checked = item.c ? "checked" : "";
    const checkedClass = item.c ? " checked" : "";
    const html = '<div class="item">' +
      '<span class="drag-handle" ontouchstart="onDragStart(event,' + index + ')" onmousedown="onDragStart(event,' + index + ')">&#x283F;</span>' +
      '<label class="checkbox-label"><input type="checkbox" ' + checked + ' onchange="toggleItem(' + index + ')"><span class="checkbox-box"></span></label>' +
      '<input type="text" class="item-text' + checkedClass + '" value="' + escapeHtml(item.n) + '" maxlength="' + MAX_NAME_BYTES + '" oninput="updateItemText(' + index + ', this.value)">' +
      '<button class="icon-btn delete-btn" onclick="deleteItem(' + index + ')">&#10005;</button>' +
      '</div>';
    container.insertAdjacentHTML("beforeend", html);
  });
  updateLimitStatus();
}

function toggleItem(index) {
  items[index].c = !items[index].c;
  const container = document.getElementById("items_list");
  const textInput = container.children[index].querySelector('.item-text');
  if (items[index].c) {
    textInput.classList.add('checked');
  } else {
    textInput.classList.remove('checked');
  }
}

function updateItemText(index, text) {
  items[index].n = text;
  updateLimitStatus();
}

function onDragStart(e, index) {
  e.preventDefault();
  const startY = e.touches ? e.touches[0].clientY : e.clientY;
  const el = document.getElementById('items_list').children[index];
  dragState = {
    index: index,
    targetIndex: index,
    startY: startY,
    itemHeight: el.getBoundingClientRect().height,
    el: el,
  };
  el.classList.add('dragging');
  document.addEventListener('touchmove', onDragMove, { passive: false });
  document.addEventListener('touchend', onDragEnd);
  document.addEventListener('mousemove', onDragMove);
  document.addEventListener('mouseup', onDragEnd);
}

function onDragMove(e) {
  if (!dragState) return;
  e.preventDefault();
  const deltaY = (e.touches ? e.touches[0].clientY : e.clientY) - dragState.startY;
  dragState.el.style.transform = 'translateY(' + deltaY + 'px)';
  const newTarget = Math.max(0, Math.min(items.length - 1,
    Math.round(dragState.index + deltaY / dragState.itemHeight)));
  if (newTarget === dragState.targetIndex) return;
  dragState.targetIndex = newTarget;
  const { index, targetIndex, itemHeight, el } = dragState;
  Array.from(document.getElementById('items_list').children).forEach(function (child, i) {
    if (child === el) return;
    if (index < targetIndex && i > index && i <= targetIndex) {
      child.style.transform = 'translateY(-' + itemHeight + 'px)';
    } else if (index > targetIndex && i >= targetIndex && i < index) {
      child.style.transform = 'translateY(' + itemHeight + 'px)';
    } else {
      child.style.transform = '';
    }
  });
}

function onDragEnd() {
  if (!dragState) return;
  document.removeEventListener('touchmove', onDragMove);
  document.removeEventListener('touchend', onDragEnd);
  document.removeEventListener('mousemove', onDragMove);
  document.removeEventListener('mouseup', onDragEnd);
  Array.from(document.getElementById('items_list').children).forEach(function (child) {
    child.style.transform = '';
    child.classList.remove('dragging');
  });
  const { index, targetIndex } = dragState;
  dragState = null;
  if (targetIndex !== index) {
    const item = items.splice(index, 1)[0];
    items.splice(targetIndex, 0, item);
    renderItems();
  }
}

function deleteItem(index) {
  items.splice(index, 1);
  renderItems();
}

function clearCompleted() {
  items = items.filter(function (item) { return !item.c; });
  renderItems();
}

function addItem() {
  const input = document.getElementById("new_item_input");
  const text = truncateName(input.value.trim());
  if (text && items.length < MAX_ITEMS) {
    items.push({ n: text, c: false });
    input.value = "";
    document.getElementById("add_btn").disabled = true;
    renderItems();
  }
}

function showIOSection(mode) {
  const section = document.getElementById("io_section");
  const textarea = document.getElementById("csv_output");
  const applyRow = document.getElementById("apply_row");

  if (section.style.display !== "none" && section.dataset.mode === mode) {
    section.style.display = "none";
    return;
  }

  if (mode === "export") {
    textarea.value = items.map(function (item) {
      return '"' + item.n.replace(/"/g, '""') + '",' + (item.c ? "1" : "0");
    }).join("\n");
    textarea.readOnly = true;
    textarea.placeholder = "";
    applyRow.style.display = "none";
    section.dataset.mode = "export";
    section.style.display = "block";
    textarea.select();
  } else {
    textarea.value = "";
    textarea.readOnly = false;
    textarea.placeholder = "Paste plaintext list or an exported CSV";
    applyRow.style.display = "block";
    section.dataset.mode = "import";
    section.style.display = "block";
    textarea.focus();
  }
}

function openSettings() {
  document.getElementById("settings_modal").style.display = "flex";
}

function closeSettings(event) {
  // ignore clicks that originate inside the modal (they bubble to the overlay)
  if (event && event.target !== event.currentTarget) return;
  document.getElementById("settings_modal").style.display = "none";
}

function toggleMenu(event) {
  event.stopPropagation();
  const isOpen = document.getElementById("menu_items").classList.toggle("open");
  document.getElementById("menu_btn").setAttribute("aria-expanded", isOpen);
}

function closeMenu() {
  document.getElementById("menu_items").classList.remove("open");
  document.getElementById("menu_btn").setAttribute("aria-expanded", "false");
}

document.addEventListener("click", closeMenu);

function exportCSV() {
  showIOSection("export");
}

function showImport() {
  showIOSection("import");
}

function applyImport() {
  const textarea = document.getElementById("csv_output");
  const lines = textarea.value.split("\n")
    .map(function (l) { return l.trim(); })
    .filter(function (l) { return l; });

  if (!lines.length) return;

  const imported = [];
  const csvPattern = /^"((?:[^"]|"")*)",(0|1)$/;
  if (csvPattern.test(lines[0])) {
    lines.forEach(function (line) {
      const match = line.match(csvPattern);
      if (match) imported.push({ n: truncateName(match[1].replace(/""/g, '"')), c: match[2] === "1" });
    });
  } else {
    lines.forEach(function (line) {
      imported.push({ n: truncateName(line), c: false });
    });
  }

  // only import what fits
  const added = imported.slice(0, Math.max(0, MAX_ITEMS - items.length));
  items = items.concat(added);

  document.getElementById("io_section").style.display = "none";
  renderItems();
  if (added.length < imported.length) {
    updateLimitStatus("Only " + added.length + " of " + imported.length +
      " items were imported. Lists are limited to " + MAX_ITEMS + " items.");
  }
}

function cancelAndClose() {
  document.location.href = getQueryParam("return_to", "pebblejs://close");
}

function submitData() {
  if (document.getElementById("save_btn").disabled) return;
  const config = { items: JSON.parse(getListJson()), settings: getSettingsBitfield() };
  const configStr = encodeURIComponent(JSON.stringify(config)).replace(/'/g, '%27');
  document.location.href = getQueryParam("return_to", "pebblejs://close#") + configStr;
}

document.getElementById("new_item_input").addEventListener("input", function () {
  document.getElementById("add_btn").disabled = !this.value.trim();
});

document.getElementById("new_item_input").addEventListener("keypress", function (e) {
  if (e.key === "Enter" && this.value.trim()) addItem();
});

window.CURRENT_STATE = __CURRENT_STATE__;
window.CURRENT_SETTINGS = __CURRENT_SETTINGS__;
window.TOTAL_ITEMS = __TOTAL_ITEMS__;
parseCurrentState();
parseCurrentSettings();
renderItems();
showTruncatedWarning();

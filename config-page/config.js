let items = [];
let dragState = null;

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
      '<input type="text" class="item-text' + checkedClass + '" value="' + escapeHtml(item.n) + '" oninput="updateItemText(' + index + ', this.value)">' +
      '<button class="icon-btn delete-btn" onclick="deleteItem(' + index + ')">&#10005;</button>' +
      '</div>';
    container.insertAdjacentHTML("beforeend", html);
  });
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
  const text = input.value.trim();
  if (text) {
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

  const csvPattern = /^"((?:[^"]|"")*)",(0|1)$/;
  if (csvPattern.test(lines[0])) {
    lines.forEach(function (line) {
      const match = line.match(csvPattern);
      if (match) items.push({ n: match[1].replace(/""/g, '"'), c: match[2] === "1" });
    });
  } else {
    lines.forEach(function (line) {
      items.push({ n: line, c: false });
    });
  }

  document.getElementById("io_section").style.display = "none";
  renderItems();
}

function cancelAndClose() {
  document.location.href = getQueryParam("return_to", "pebblejs://close");
}

function submitData() {
  const config = { itemUpdates: [] };
  items.forEach(function (item) {
    config.itemUpdates.push({ name: item.n, checked: item.c, action: "update" });
  });
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
parseCurrentState();
renderItems();

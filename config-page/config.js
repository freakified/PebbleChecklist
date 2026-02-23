let items = [];

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
  catch(e) { items = []; }
}

function escapeHtml(text) {
  const div = document.createElement("div");
  div.textContent = text;
  return div.innerHTML.replace(/"/g, '&quot;');
}

function renderItems() {
  const container = document.getElementById("items_list");
  container.innerHTML = "";
  items.forEach(function(item, index) {
    const checked = item.c ? "checked" : "";
    const checkedClass = item.c ? " checked" : "";
    const html = '<div class="item">' +
      '<input type="checkbox" ' + checked + ' onchange="toggleItem(' + index + ')">' +
      '<input type="text" class="item-text' + checkedClass + '" value="' + escapeHtml(item.n) + '" oninput="updateItemText(' + index + ', this.value)">' +
      '<button class="icon-btn move-btn" onclick="moveToTop(' + index + ')">&#8593;</button>' +
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

function moveToTop(index) {
  const item = items.splice(index, 1)[0];
  items.unshift(item);
  renderItems();
}

function deleteItem(index) {
  items.splice(index, 1);
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

function exportCSV() {
  const csv = items.map(function(item) {
    return '"' + item.n.replace(/"/g, '""') + '",' + (item.c ? "1" : "0");
  }).join("\n");
  const csvArea = document.getElementById("csv_output");
  if (csvArea) {
    csvArea.value = csv;
    csvArea.style.display = "block";
    csvArea.select();
  } else {
    const blob = new Blob([csv], { type: "text/csv" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = "checklist.csv";
    a.click();
    URL.revokeObjectURL(url);
  }
}

function cancelAndClose() {
  document.location.href = getQueryParam("return_to", "pebblejs://close");
}

function submitData() {
  const config = { itemUpdates: [] };
  items.forEach(function(item) {
    config.itemUpdates.push({ name: item.n, checked: item.c, action: "update" });
  });
  document.location.href = getQueryParam("return_to", "pebblejs://close#") + encodeURIComponent(JSON.stringify(config));
}

document.getElementById("new_item_input").addEventListener("input", function() {
  document.getElementById("add_btn").disabled = !this.value.trim();
});

document.getElementById("new_item_input").addEventListener("keypress", function(e) {
  if (e.key === "Enter" && this.value.trim()) addItem();
});

window.CURRENT_STATE=__CURRENT_STATE__;
parseCurrentState();
renderItems();

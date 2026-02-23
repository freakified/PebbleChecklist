var configUri = require('./configDataUri');

Pebble.addEventListener('ready', function () {
  console.log('JS component loaded!');
});

Pebble.addEventListener('showConfiguration', function () {
  console.log('Requesting current state from watch');
  Pebble.sendAppMessage({ 1: 1 }, function () {
    console.log('State request sent');
  }, function () {
    openConfigPage();
  });
});

function openConfigPage(currentState) {
  var url = configUri;
  if (currentState) {
    url = url.replace('__CURRENT_STATE__', encodeURIComponent(JSON.stringify(currentState)));
  }
  Pebble.openURL(url);
}

Pebble.addEventListener('appmessage', function (e) {
  if (e.payload[2]) openConfigPage(e.payload[2]);
});

Pebble.addEventListener('webviewclosed', function (e) {
  var data = e.response ? JSON.parse(e.response) : null;
  if (!data) return console.log('No settings changed');

  var dict = {};
  if (data.itemsToAdd) dict[0] = data.itemsToAdd;
  if (data.itemUpdates) dict[3] = JSON.stringify(data.itemUpdates);

  Pebble.sendAppMessage(dict, function () {
    console.log('Sent config data to Pebble');
  }, function () {
    console.log('Failed to send config data');
  });
});

// var BASE_CONFIG_URL = 'http://localhost:4000/';
// var BASE_CONFIG_URL = 'http://192.168.0.103:4000/';
var BASE_CONFIG_URL = 'http://freakified.github.io/PebbleChecklist/';

Pebble.addEventListener('ready', function(e) {
  console.log('JS component loaded!');
});

// open the config page when requested
Pebble.addEventListener('showConfiguration', function(e) {
  var configURL = BASE_CONFIG_URL + 'config.html';

  Pebble.openURL(configURL);
});

// react to the config page when new data is sent
Pebble.addEventListener('webviewclosed', function(e) {
  var configData = decodeURIComponent(e.response);

  if(configData) {
    configData = JSON.parse(decodeURIComponent(e.response));

    console.log("Config data recieved!" + JSON.stringify(configData));

    // prepare a structure to hold everything we'll send to the watch
    var dict = {};

    // color settings
    if(configData.itemsToAdd) {
      dict.KEY_ITEMS_TO_ADD = configData.itemsToAdd;
    }

    console.log('Preparing message: ', JSON.stringify(dict));

    // Send settings to Pebble watchapp
    Pebble.sendAppMessage(dict, function(){
      console.log('Sent config data to Pebble');
    }, function() {
      console.log('Failed to send config data!');
    });
  } else {
    console.log("No settings changed!");
  }
});

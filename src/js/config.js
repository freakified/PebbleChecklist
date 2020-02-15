// var BASE_CONFIG_URL = 'http://localhost:4000/';
// var BASE_CONFIG_URL = 'http://192.168.0.103:4000/';
var BASE_CONFIG_URL = 'http://clach04.github.io/pebble/checklist/';


Pebble.addEventListener('ready', function(e) {
  console.log('JS component loaded!');
});

// open the config page when requested
Pebble.addEventListener('showConfiguration', function(e) {
  var configURL = BASE_CONFIG_URL + 'config.html';
  var items_exported = localStorage.getItem('items_exported') || '[]';

    try {
      items_exported = JSON.parse(items_exported);
    }
    catch(error) {
      console.error('items_exported parse failed, defaulting to [] - ' + error);
      // expected output: ReferenceError: nonExistentFunction is not defined
      // Note - error messages will vary depending on browser
      items_exported = [];
    }

  console.log('config items_exported = ', JSON.stringify(items_exported));
  //configURL = configURL + '?items_exported=test,notes,here';
  //configURL = configURL + '?items_exported=' + encodeURIComponent(items_exported.join(','));
  configURL = configURL + '?items_exported=' + items_exported.join(',');

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
      //itemsToAdd = [];  // reset export list
    }, function() {
      console.log('Failed to send config data!');
    });
  } else {
    console.log("No settings changed!");
  }
});

Pebble.addEventListener("appmessage", function(e) {
   if (e.payload.KEY_ITEMS_TO_ADD) {
      var items_exported = [];

      console.log('Message from Pebble: ' + JSON.stringify(e.payload));
      console.log('Message from Pebble value: ' + e.payload.KEY_ITEMS_TO_ADD);
      console.log('pre add items_exported = ', JSON.stringify(items_exported));
      items_exported.push(e.payload.KEY_ITEMS_TO_ADD);
      console.log('post add items_exported = ', JSON.stringify(items_exported));
      //items_exported.push('static');
      //console.log('post add static items_exported = ', JSON.stringify(items_exported));
      localStorage.setItem('items_exported', JSON.stringify(items_exported));  // global variables do not persist even when watch app is still running, so store now
   }
});

var config = require('./config');

// ES5-compatible minification function
function minifyConfig(html) {
  return html
    // Remove HTML comments
    .replace(/<!--[\s\S]*?-->/g, '')
    // Remove line breaks first
    .replace(/\n/g, '')
    // Remove spaces around semicolons and braces
    .replace(/\s*;\s*/g, ';')
    .replace(/\s*\{\s*/g, '{')
    .replace(/\s*\}\s*/g, '}')
    // Remove spaces between tags
    .replace(/>\s+</g, '><')
    // Remove multiple spaces
    .replace(/\s+/g, ' ')
    // Remove leading/trailing spaces
    .trim();
}

// Build complete HTML template
function getConfigHTML() {
  var htmlParts = [
    '<!DOCTYPE html>',
    '<html>',
    '<head>',
    '  <meta charset="UTF-8">',
    '  <meta name="viewport" content="width=device-width,initial-scale=1.0">',
    '  <title>Checklist</title>',
    '  <style>',
    config.CONFIG_CSS_TEMPLATE,
    '  </style>',
    '</head>',
    '<body>',
    config.CONFIG_BODY_TEMPLATE,
    config.CONFIG_SCRIPT_TEMPLATE,
    '</body>',
    '</html>'
  ];

  return htmlParts.join('\n');
}

Pebble.addEventListener('ready', function (e) {
  console.log('JS component loaded!');
});

// open the config page when requested
Pebble.addEventListener('showConfiguration', function (e) {
  console.log('Requesting current state from watch');
  
  // Request current state from watch
  Pebble.sendAppMessage({ 1: 1 }, function() {
    console.log('State request sent successfully');
  }, function() {
    console.log('Failed to send state request, opening config without current data');
    openConfigPage();
  });
});

// Function to open config page with optional current state
function openConfigPage(currentState) {
  // Build readable template (skip minification for debugging)
  var readableHTML = getConfigHTML();
  
  // Embed current state directly into HTML if provided
  if (currentState) {
    readableHTML = readableHTML.replace(
      '// Initialize the page',
      '// Initialize the page\nwindow.CURRENT_STATE = ' + JSON.stringify(currentState) + ';'
    );
  }

  console.log('Original size:', readableHTML.length);
  console.log('Opening config page');
  
  // Use non-minified version for debugging
  var configURL = 'data:text/html;charset=utf-8,' + encodeURIComponent(readableHTML);
  
  Pebble.openURL(configURL);
}

// Handle current state from watch
Pebble.addEventListener('appmessage', function(e) {
  console.log('AppMessage received: ', JSON.stringify(e.payload));
  
  if (e.payload[2]) {
    console.log('Current state received, opening config page');
    openConfigPage(e.payload[2]);
  }
});

// react to the config page when new data is sent
Pebble.addEventListener('webviewclosed', function (e) {
  var configData = decodeURIComponent(e.response);

  if (configData) {
    configData = JSON.parse(decodeURIComponent(e.response));

    console.log("Config data recieved!" + JSON.stringify(configData));

    // prepare a structure to hold everything we'll send to the watch
    var dict = {};

    // Handle legacy itemsToAdd for backward compatibility
    if (configData.itemsToAdd) {
      dict[0] = configData.itemsToAdd; // KEY_ITEMS_TO_ADD
    }

    // Handle new itemUpdates format
    if (configData.itemUpdates) {
      dict[3] = JSON.stringify(configData.itemUpdates); // KEY_ITEM_UPDATES
    }

    console.log('Preparing message: ', JSON.stringify(dict));

    // Send settings to Pebble watchapp
    Pebble.sendAppMessage(dict, function () {
      console.log('Sent config data to Pebble');
    }, function () {
      console.log('Failed to send config data!');
    });
  } else {
    console.log("No settings changed!");
  }
});

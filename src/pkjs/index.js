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
  // Build readable template and minify at runtime
  var readableHTML = getConfigHTML();
  var minifiedHTML = minifyConfig(readableHTML);

  console.log('Original size:', readableHTML.length);
  console.log('Minified size:', minifiedHTML.length);
  console.log('Opening URL-encoded config page');

  // Try URL-encoded config first
  var configURL = 'data:text/html;charset=utf-8,' + encodeURIComponent(minifiedHTML);
  Pebble.openURL(configURL);

  // Fallback to hosted version if needed (uncomment to enable)
  // setTimeout(function() {
  //   console.log('Falling back to hosted config page');
  //   Pebble.openURL(config.BASE_CONFIG_URL + 'config.html');
  // }, 1000);
});

// react to the config page when new data is sent
Pebble.addEventListener('webviewclosed', function (e) {
  var configData = decodeURIComponent(e.response);

  if (configData) {
    configData = JSON.parse(decodeURIComponent(e.response));

    console.log("Config data recieved!" + JSON.stringify(configData));

    // prepare a structure to hold everything we'll send to the watch
    var dict = {};

    // color settings
    if (configData.itemsToAdd) {
      dict.KEY_ITEMS_TO_ADD = configData.itemsToAdd;
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

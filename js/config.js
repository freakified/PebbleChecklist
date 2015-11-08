$(document).on('click', updateSubmitButtonVisibility);
$('#title').on('click', cancelAndClose);
$('#submit_button').on('click', sendDataToWatch);

function updateSubmitButtonVisibility() {
  var $items = $('#items_to_add').children('.item:not(.add-item)');
  var count = 0;

  $items.each(function() {
    if($(this).text() != '') {
      count++;
    }
  });

  if(count > 0) {
    $('#submit_button').removeClass('disabled');
  } else {
    $('#submit_button').addClass('disabled');
  }
}

function getQueryParam(variable, defaultValue) {
  // Find all URL parameters
  var query = location.search.substring(1);
  var vars = query.split('&');
  for (var i = 0; i < vars.length; i++) {
    var pair = vars[i].split('=');

    // If the query variable parameter is found, decode it to use and return it for use
    if (pair[0] === variable) {
      return decodeURIComponent(pair[1]);
    }
  }
  return defaultValue || false;
}

function cancelAndClose() {
  var return_to = getQueryParam('return_to', 'pebblejs://close');
  document.location.href = return_to;
}

function sendDataToWatch() {
  // stick all the settings into a JSON object
  var config = {};

  var $items = $('#items_to_add').children('.item:not(.add-item)');
  var addString = '';

  $items.each(function() {
    addString += $(this).text() + '.';
  });

  config.addString = addString;

  // Set the return URL depending on the runtime environment
  var return_to = getQueryParam('return_to', 'pebblejs://close#');
  document.location.href = return_to + encodeURIComponent(JSON.stringify(config));
}

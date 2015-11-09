// reverse function from http://stackoverflow.com/questions/1394020/jquery-each-backwards
Zepto.fn.reverse = [].reverse;

$('#items_to_add').on('change', updateSubmitButtonVisibility);
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
    var buttonText = 'SEND ' + count + ' ITEM' + ((count > 1) ? 'S' : '') + ' TO WATCH';
    $('#submit_button').attr('value', buttonText);
    $('#submit_button').removeClass('hidden');
  } else {
    $('#submit_button').addClass('hidden');
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
  var itemsToAdd = '';

  $items.reverse().each(function() {
    itemsToAdd += $(this).text() + '.';
  });

  config.itemsToAdd = itemsToAdd;

  // Set the return URL depending on the runtime environment
  var return_to = getQueryParam('return_to', 'pebblejs://close#');
  document.location.href = return_to + encodeURIComponent(JSON.stringify(config));
}

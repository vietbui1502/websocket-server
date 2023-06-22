// JSON-RPC over Websocket implementation
var JSONRPC_TIMEOUT_MS = 1000;

var jsonrpc = function(url, onopen, onclose, onnotification) {
  var rpcid = 0, pending = {}, ws = new WebSocket(url);
  if (!ws) return null;
  ws.onclose = onclose;
  ws.onmessage = function(ev) {
    const frame = JSON.parse(ev.data);
    console.log('rcvd', frame, 'pending:', pending);
    if (frame.id !== undefined) {
      if (pending[frame.id] !== undefined) pending[frame.id](frame);  // Resolve
      delete (pending[frame.id]);
    } else {
      if (onnotification) onnotification(frame);
    }
  };
  if (onopen) onopen();
  return {
    close: () => ws.close(),
    call: function(method, params) {
      const id = rpcid++, request = {id, method, params};
      ws.send(JSON.stringify(request));
      console.log('sent', request);
      return new Promise(function(resolve, reject) {
        setTimeout(function() {
          if (pending[id] === undefined) return;
          log('Timing out frame ', JSON.stringify(request));
          delete (pending[id]);
          reject();
        }, JSONRPC_TIMEOUT_MS);
        pending[id] = x => resolve(x);
      });
    },
  };
};

function isValidMACAddress(str) {
  // Regex to check valid
  // MAC_Address 
  let regex = new RegExp(/^([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})|([0-9a-fA-F]{4}.[0-9a-fA-F]{4}.[0-9a-fA-F]{4})$/);

  // if str
  // is empty return false
  if (str == null) {
      return "false";
  }

  // Return true if the str
  // matched the ReGex
  if (regex.test(str) == true) {
      return "true";
  }
  else {
      return "false";
  }
}

function isValidSchedule(str) {
  // Regex to check valid
  // MAC_Address 
  let regex = new RegExp(/^(0[0-9]|1\d|2[0-3]){1}-(0[0-9]|1\d|2[0-3]){1}$/);

  // if str
  // is empty return false
  if (str == null) {
      return "false";
  }

  // Return true if the str
  // matched the ReGex
  if (regex.test(str) == true) {
      return "true";
  }
  else {
      return "false";
  }
}

const WebSocket = require('ws');

const express = require('express')
const app = express()
const port1 = 3000

var num_data = 0;
var start = new Date()
var simulateTime = 1000;
                                                                                 
console.log("Trying to connect websocket");
const wss = new WebSocket.Server({ host: "192.168.137.1", port: 8000 });
console.log("...done!");

wss.on('connection', function connection(ws) {
  console.log('Nova connexió de l\'adreça remota ' + ws._socket.remoteAddress + ':' + ws._socket.remotePort);

  ws.on('message', function incoming(message) {
    console.log(message.toString());
 
    wss.clients.forEach(function each(client) {
      if (client.readyState === WebSocket.OPEN) {
        client.send(message.toString());
      }
    });
  });
});


app.get('/', (req, res) => {
    res.sendFile(`${__dirname}/ambient.html`)
})

app.listen(port1, () => {
    console.log(`Example app listening on port ${port1}`)
})

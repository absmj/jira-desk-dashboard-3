// Prints the frames the REAL web app code (web/protocol.js) produces for the repo's data files,
// one hex frame per line, "<file> <hex>". test_ble.cpp feeds them to the firmware's TransferSession,
// so the two sides are checked against each other, not against a copy of the protocol.
const fs = require('fs'), path = require('path');
const P = require('../../web/protocol.js');
for (const [id, name] of [[P.FILE.sprint, 'sprint.json'], [P.FILE.alerts, 'alerts.json']]) {
  const text = fs.readFileSync(path.join(__dirname, '../../data/device', name), 'utf8');
  for (const f of P.buildFrames(id, text)) console.log(name + ' ' + Buffer.from(f).toString('hex'));
}

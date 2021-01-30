const fs = require('fs');
const addon = require('./build/Release/thing');

const { TunInterface } = addon;

const tap = new TunInterface({
    // optional, kernel will automatically assign a name if not given here
  name: 'cstap0',
  // can be either "tun" or "tap", default is "tun"
  // tun mode gets you ip packets, tap mode gets you ethernet frames
  mode: 'tap',
  // set to true if you want the 4-byte packet information header
  // default is false, which adds IFF_NO_PI to ifr_flags
  pi: false
});

console.log(tap.setAddress("10.0.0.1"));

// let readStream = fs.createReadStream(null, { fd: tap.fd });
// readStream.on('data', (data) => {
//     console.log(data);
// });

module.exports = addon;

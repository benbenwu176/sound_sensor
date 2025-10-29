const {SerialPort} = require('serialport');

// Example usage
const deviceName = process.argv[2];
const message = 'Hello from Ben!';

const port = new SerialPort({path: deviceName, baudRate: 115200});
let old_mask = 0;

port.write(message, (error) => {
  if (error) {
    console.error(`Error writing to serial port: ${error}`);
  } else {
    console.log('Message sent successfully');
  }
});

port.on('data', (data) => {
    console.log(data);
//   let device_id = data[0];
//   let new_mask = data[1];
//   let diff = new_mask ^ old_mask;
//   for (let sensor_id = 0; sensor_id < 6; sensor_id++) {
//     let pad_mask = diff & (1 << sensor_id);
//     if (diff & pad_mask) {
//         let state_on = (new_mask & pad_mask) ? true : false;
//         console.log(`${sensor_id} ${state_on}`);
//     }
//   }
//   old_mask = new_mask;
});

port.on('error', (error) => {
  console.error(`Error opening serial port: ${error}`);
});
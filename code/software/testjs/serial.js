const {SerialPort} = require('serialport');

// Example usage
const deviceName = process.argv[2];
const message = 'Hello from Ben!';

const port = new SerialPort({path: deviceName, baudRate: 115200});

port.write(message, (error) => {
  if (error) {
    console.error(`Error writing to serial port: ${error}`);
  } else {
    console.log('Message sent successfully');
  }
});

port.on('data', (data) => {
  let output = "";
  for (const byte of data) {
      output += byte + " ";
  }
  console.log(output);
});

port.on('error', (error) => {
  console.error(`Error opening serial port: ${error}`);
});
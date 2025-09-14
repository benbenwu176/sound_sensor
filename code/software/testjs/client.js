const {SerialPort} = require('serialport');

function sendData(deviceName, message) {
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
}

// Example usage
const deviceName = 'COM5'; // Replace with the actual device name
const message = 'Hello from Ben!';
sendData(deviceName, message);
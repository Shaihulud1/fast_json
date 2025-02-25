# Fast JSON Parser

High-performance asynchronous JSON parser for Node.js

## Installation

```bash
npm install fast-json-parser
```

## Usage

```javascript
const fastJsonParser = require('fast-json-parser');

// Using Promise
fastJsonParser.parseAsync('{"key": "value"}')
  .then(result => console.log(result))
  .catch(err => console.error(err));

// Using callback
fastJsonParser.parseAsync('{"key": "value"}', (err, result) => {
  if (err) {
    console.error(err);
  } else {
    console.log(result);
  }
});
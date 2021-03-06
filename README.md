# node-ngspice
Node ❤ NGSPICE. (Frontend for NGSPICE)

[![Build Status](https://travis-ci.org/Higuoxing/node-ngspice.svg?branch=master)](https://travis-ci.org/Higuoxing/node-ngspice)

> A simple frontend IDE based on Node.js for *NGSPICE* users :) enjoy it!

### Screenshots

![screenshot](./.screenshot/screenshot.jpg)

### Build& Deploy

```bash
$ git clone https://github.com/higuoxing/node-ngspice.git
$ cd node-ngspice
$ npm install
$ node ./bin/www
```

### How to use

This application is to plot vectors in ngspice. The plot option should be written in JSON form.

##### Example

```javascript
[{ "name": "v(in)"   , "curve": "v(in)"   },
 { "name": "v(out)"  , "curve": "v(out)"  },
 { "name": "v(4, 5)" , "curve": "v(4, 5)" } ]
```

`name` : is the displayed name in plot legend
`curve`: is the vector variable in ngspice

### Appreciation

##### Standing on the shoulders of giants!

* Node.js
* Bootstrap
* Express
* Plotly.js
* NGSPICE

And special thanks to [Online-NgSpice-Simulator](https://github.com/FOSSEE/Online-NgSpice-Simulator)

### TODO

- [ ] Server message implement
- [ ] Full documents
- [ ] Code highlight
- [ ] Rewrite using `Vue.js`

### Contribution

This is a `Learn by Doing` project, and I want to modified it using `Vue.js`... And if you are interested in this project, please feel free to contact me! <higuoxing@gmail.com> :)

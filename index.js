const express = require('express')
const bp = require('body-parser')
const lts = require("log-timestamp")
const md5 = require("md5")
const app = express()
const fs = require("fs")
var privateKey  = fs.readFileSync('sslcert/vheavy.key', 'utf8');
var certificate = fs.readFileSync('sslcert/vheavy_com.pem', 'utf8');
const https = require("https")
const http = require("http")
const morgan = require('morgan')

var credentials = {key: privateKey, cert: certificate};

const { spawn } = require('child_process');
const execSync = require('child_process').execSync;

var httpServer = http.createServer(app);
var httpsServer = https.createServer(credentials, app);

app.use(bp.json())
app.use(bp.urlencoded({ extended: true }))
//app.use(morgan("combined"))

app.get('/', (req, res) => {
    const ps = spawn('sh', ['test.sh']);
    res.writeHead(200, {'Content-Type': 'text/html'});
    ps.stdout.pipe(res)
})
app.get('/graph', (req, res) => {
  execSync('sudo -iu ec2-user /home/ec2-user/simplePost/graph.sh');
  res.sendFile('p.png', { root: __dirname });
});
app.get('/fils', (req, res) => {
  var f = req.query.f;
  res.sendFile(f, { root: __dirname });
});

app.get('/ota', (req, res) => {
  var SIZE = parseInt(req.query.len); // size of file rad 
  var offset = parseInt(req.query.offset); // offset of file read 
	console.log('OTA offset %d len %d', offset, SIZE)
  fs.open("./firmware.bin", 'r', function(err, fd) {
    fs.fstat(fd, function(err, stats) {
      var buffer = new Buffer.alloc(SIZE);
      var r = fs.readSync(fd, buffer, 0, SIZE, offset);
      res.set("Connection", "close"); // Note: this is an HTTP header.
      res.set("Content-Length", r * 2);
      res.writeHead(200, {'Content-Type': 'text/plain'});
          res.write(buffer.toString('hex', 0, r));
    });
  });
})

app.post('/log', (req, res) => {
	console.log(JSON.stringify(req.body, null, 0))
	const ver = fs.readFileSync("firmware.ver").toString().replace(/\s/g, '')
	res.json({ "sleep":22, "ota_ver" : ver, "status" : 1, "pwm":1050, "pwmEnd":1250 })
})

httpServer.listen(80);
httpsServer.listen(443);


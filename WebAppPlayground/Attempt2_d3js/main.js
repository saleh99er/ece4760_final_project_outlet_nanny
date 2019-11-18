const max_sample_freq = 10000; //setting to 10k for now
const min_sample_freq = 120; //nyquist rate, 60hz signal, 120 hz sampling freq
// AT LEAST required

function myFunction() {
  document.getElementById("demo").innerHTML = "ECE 4760 Rules!!!";
}

function turnOnRelay() {
  console.log("turning on relay...");
}

function turnOffRelay() {
  console.log("turning off relay...");
}

function parseUserInput() {
  console.log("parsing user input...");

  //sample freq
  var sample_rate = document.getElementById("sample-rate").value;
  if (sample_rate < min_sample_freq) {
    console.error("sample rate too low");
  }
  else if (sample_rate > max_sample_freq) {
    console.error("sample rate too high");
  }
  else {
    console.log(" sample rate = " + sample_rate);
  }

}

function attemptLine() {
  // create data
  var data = [{ x: 0, y: 20 }, { x: 150, y: 150 }, { x: 300, y: 100 }, { x: 450, y: 20 }, { x: 600, y: 130 }]

  // create svg element:
  var svg = d3.select("#line").append("svg").attr("width", 800).attr("height", 200)

  // prepare a helper function
  var lineFunc = d3.line()
    .x(function (d) { return d.x })
    .y(function (d) { return d.y })

  // Add the path using this helper function
  svg.append('path')
    .attr('d', lineFunc(data))
    .attr('stroke', 'black')
    .attr('fill', 'none');
}
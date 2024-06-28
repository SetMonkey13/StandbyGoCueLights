const char MAIN_page[] PROGMEM = R"=====(
<!DOCTYPE html>

<head>
</head>
<html>
<style>
  mark::before,
  mark::after,
  del::before,
  del::after,
  ins::before,
  ins::after,
  s::before,
  s::after {
    clip-path: inset(100%);
    clip: rect(1px, 1px, 1px, 1px);
    height: 1px;
    width: 1px;
    overflow: hidden;
    position: absolute;
    white-space: nowrap;
  }

  body {
    font-family: Arial, Helvetica, sans-serif;
    color: black;
  }

  a:link {
    color: firebrick;
    text-decoration: none;
  }

  a:hover {
    color: darkred;
    text-decoration: none;
  }

  * {
    box-sizing: border-box;
  }

  h3 {
    margin-bottom: 0px;
  }

  .detail {
    margin-top: 0px;
    font-size: .8em;
    margin-bottom: 0px;
    margin-left: 12px;
    color: gray;
  }

  .valuetitle {
    margin-bottom: .5em;
  }

  .currentvalues {
    float: right;
    text-align: right;
    color: grey;
  }

  .form-inline {
    display: flex;
    flex-flow: column;
    align-items: left;
  }

  .form-inline label {
    margin: 5px 10px 5px 0;
    display: block;
    font-size: 1em;
  }

  .form-inline input {
    vertical-align: middle;
    margin: 5px 10px 5px 0;
    padding: 12px 20px;
    background-color: #fff;
    border: 1px solid #ccc;
    border-radius: 10px;
    width: 100%;
  }

  input[type='radio'] {
    width: auto;
    vertical-align: baseline;
    margin: 15px 0px 5px 0;

  }

  input[type='radio']+label {
    display: inline;
    width: auto;
    margin: 15px 10px 5px .01em;

  }

  input[type='color'] {
    height: 60px;
  }

  .form-inline button {
    padding: 10px 20px;
    background-color: FireBrick;
    border: 1px solid #ddd;
    color: white;
    cursor: pointer;

  }

  .form-inline button:hover {
    background-color: darkred;
  }

  @media (max-width: 900px) {
    .form-inline input {
      margin: 10px 0;
    }

    .form-inline {
      flex-direction: column;
      align-items: stretch;
      width: 100%;
    }

    .row {
      flex-direction: column;
    }

    .col-25 {
      margin-bottom: 20px;
    }
  }

  .row {
    display: -ms-flexbox;
    /* IE10 */
    display: flex;
    -ms-flex-wrap: wrap;
    /* IE10 */
    flex-wrap: wrap;
    margin: 0 -16px;
  }

  .row2 {
    display: -ms-flexbox;
    /* IE10 */
    display: flex;
    -ms-flex-wrap: nowrap;
    /* IE10 */
    flex-wrap: nowrap;
    margin: 0 -16px;
  }

  .col-25 {
    -ms-flex: 25%;
    /* IE10 */
    flex: 25%;
  }

  .col-50 {
    -ms-flex: 50%;
    /* IE10 */
    flex: 50%;
  }

  .col-75 {
    -ms-flex: 75%;
    /* IE10 */
    flex: 75%;
  }

  .col-100 {
    -ms-flex: 100%;
    /* IE10 */
    flex: 100%;
  }

  .col-25,
  .col-50,
  .col-75,
  .col-100 {
    padding: 0 16px;
  }

  .container {
    background-color: #f2f2f2;
    padding: 5px 20px 15px 20px;
    border: 1px solid lightgrey;
    border-radius: 3px;
  }

  #content {
    display: "none";

  }
</style>
<title>Config Page</title>

<body>
  <div class="row">
    <div class="col-75">
      <div class="container">
        <form action='/' method='POST' class="form-inline"
          onsubmit='setTimeout(function () { window.location.reload(); }, 50)'>
          <div class="row">
            <div class="col-100">
              <h1 style="text-align:center;">StandByGO</h1>
              <h2 style="text-align:center;">A Cue Light Main Device</h2>
            </div>
          </div>
          <div class="row">
            <div class="col-100">
              <h3>Updatable Values</h3>
            </div>
          </div><br>
          <div class="row">
            <div class="col-25">
              <label for="WARN">Warning Color: @@warncolor@@<span class="detail"></span></label>
              <input type='color' name='WARN' value="@@warncolor@@">
            </div>
            <div class="col-25">
              <label for="STANDBY">Stand By Color: @@standcolor@@<span class="detail"></span></label>
              <input type='color' name='STAND' value="@@standcolor@@">
            </div>
            <div class="col-25">
              <label for="GO">Go Color: @@gocolor@@<span class="detail"></span></label>
              <input type='color' name='GO' value="@@gocolor@@">
            </div>
          </div>
          <div class="row">
            <div class="col-25"><label for="BRIGHTNESS">Brightness<span class="detail">0-255</span></label>
              <input type='number' name='BRIGHTNESS' placeholder='Brightness' min="0" max="255" value="@@brightness@@">
            </div>
          </div>

          <button type="submit" name="SUBMIT">Save Changes</button>
        </form>
      </div>
    </div>
  </div>
</body>

</html>)=====";
/*
// Default WiFi credentials and color palette
const char* DEFAULT_SSID = "MyWiFiNetwork";
const char* DEFAULT_PASSWORD = "MyWiFiPassword";
CRGB warningColor = "DarkBlue";
CRGB warningAColor = "Blue";
CRGB standbyColor = "Red";
CRGB goColor = "Green";
*/

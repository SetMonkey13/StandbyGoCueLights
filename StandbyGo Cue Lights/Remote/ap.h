const char AP_page[] PROGMEM = R"=====(
<style>
  body {
    font-family: Arial, Helvetica, sans-serif;
  }

  * {
    box-sizing: border-box;
  }

  .form-inline {
    display: flex;
    flex-flow: row wrap;
    align-items: center;
  }

  .form-inline label {
    margin: 5px 10px 5px 0;
  }

  .form-inline input {
    vertical-align: middle;
    margin: 5px 10px 5px 0;
    padding: 10px;
    background-color: #fff;
    border: 1px solid #ddd;
  }

  .form-inline button {
    padding: 10px 20px;
    background-color: dodgerblue;
    border: 1px solid #ddd;
    color: white;
    cursor: pointer;
  }

  .form-inline button:hover {
    background-color: royalblue;
  }

  @media (max-width: 800px) {
    .form-inline input {
      margin: 10px 0;
    }

    .form-inline {
      flex-direction: column;
      align-items: stretch;
    }
  }

  #text {
    display: none;
    color: red;
    margin: 0
  }
</style>
</head>

<body>
  <p>macAddress: @@mac@@</p>
  <form class="form-inline" action="/ap" method="POST">
    <div>
      <label for="SSID">SSID:</label>
      <input type="text" id="SSID" name="SSID" placeholder="SSID of Wi-Fi">
      <label for="password">Password:</label>
      <input type="password" id="password" name="PASSWORD" placeholder="Wi-Fi password">
      <input type="checkbox" onclick="myFunction()"><label>Show Password</label>
      <span id="text">WARNING! Caps lock is ON.</span>
      <script>
        function myFunction() {
          var x = document.getElementById("password");
          if (x.type === "password") {
            x.type = "text";
          } else {
            x.type = "password";
          }
        }
        var input = document.getElementById("password");
        var text = document.getElementById("text");
        input.addEventListener("keyup", function (event) {

          if (event.getModifierState("CapsLock")) {
            text.style.display = "block";
          } else {
            text.style.display = "none"
          }
        });
      </script>

      <button style="display:block;" type="submit" name="SUBMIT" value="Update SSID & Password and restart">Update
        SSID & Password and restart</button>
  </form>
</body>

</html>
)=====";
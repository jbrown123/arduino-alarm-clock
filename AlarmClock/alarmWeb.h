R"EOS(<!DOCTYPE html>
<html>
  <head>
    <meta charset="utf-8">
    <meta http-equiv="X-UA-Compatible" content="IE=edge">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no" />
    <link rel="stylesheet" href="https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css">
  </head>
  <body>
    <div class="container-fluid">
      <div class="col-xs-12"  style="height: 100vh">
        <h1>Alarm Clock - Set your alarm</h1>
        <input id="time" type="time">
        <div class="row" padding-bottom:1em">
          <div class="col-xs-4" style="height: 100%; text-align: center">
            <button id="sendAlarm" type="button" class="btn btn-default" style="height: 100%; width: 100%" onclick='saveAlarm()'>Send Alarm</button>
          </div>
        </div>
        <div class="row" padding-bottom:1em">
          <div class="col-xs-4" id="currentAlarm" style="font-size: 30px"></div>
        </div>
      </div>
    </div>
    <script src="https://ajax.googleapis.com/ajax/libs/jquery/1.12.4/jquery.min.js"></script>
    <script src="https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.min.js"></script> 
    <script> function makeAjaxCall(url){$.ajax({"url": url})}</script>
    <script> function saveAlarm()
             {
                var alarmTime = $( "#time" ).val();
                var dayNum = 1;
                if(alarmTime) {
                  console.log(alarmTime);
                  makeAjaxCall("setAlarm?alarm=" + alarmTime + "&day=" + dayNum);
                }
             }

             window.setInterval(getAlarm, 2000);


             function displayAlarm(data){
              var text = "Alarm currently set to: " + data;
               $("#currentAlarm").html(data);
             }

             function getAlarm(){
                $.ajax({"url": "getAlarm",
                "success": displayAlarm});
             }
    </script>
  </body>
</html>)EOS"

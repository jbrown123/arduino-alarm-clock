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
        <table border=0>
        <tr><td>Sun</td><td><input id="time0" type="time"></td></tr>
        <tr><td>Mon</td><td><input id="time1" type="time"></td></tr>
        <tr><td>Tue</td><td><input id="time2" type="time"></td></tr>
        <tr><td>Wed</td><td><input id="time3" type="time"></td></tr>
        <tr><td>Thu</td><td><input id="time4" type="time"></td></tr>
        <tr><td>Fri</td><td><input id="time5" type="time"></td></tr>
        <tr><td>Sat</td><td><input id="time6" type="time"></td></tr>
        </table>
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
    <script> function saveAlarm()
             {
             	var dayNum = 0;

				function nextAlarm()
             	{
             		if (dayNum >= 7)
             			return;

             		var dy = dayNum++;
             		
	                var alarmTime = $( "#time"+dy ).val();
					console.log("day: " + dy + " alarm: " + alarmTime);
	                if(alarmTime)
						$.ajax({"url": "setAlarm?alarm=" + alarmTime + ";" + dy, "success": nextAlarm});
					else
						$.ajax({"url": "setAlarm?alarm=0:0;" + dy, "success": nextAlarm});
             	}
             	
             	nextAlarm();	// kick us off
             }

             window.setInterval(getAlarm, 2000);


             function displayAlarm(data){
              var text = "Alarm currently set to: " + data;
               $("#currentAlarm").html(text);
             }

             function getAlarm(){
                $.ajax({"url": "getAlarm",
                "success": displayAlarm});
             }
    </script>
  </body>
</html>)EOS"

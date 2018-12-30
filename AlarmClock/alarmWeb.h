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
      <div class="col-xs-12"  padding-bottom:"1em">
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
        <div class="row" padding-bottom:"1em">
			Alarm sound: <input id="alarmSound" type="text">
        </div>
        <div class="row" padding-bottom:"1em">
			Volume: <input id="volume" type="number">
        </div>
        <div class="row" padding-bottom:"1em">
          <div class="col-xs-4" style="height: 100%; text-align: center">
            <button id="sendAlarm" type="button" class="btn btn-default" style="height: 100%; width: 100%" onclick='saveAlarm()'>Set Alarms</button>
          </div>
        </div>
        <div class="row" padding-bottom:"1em">
          <div class="col-xs-4" id="status" style="font-size: 30px"></div>
        </div>
      </div>
    </div>
    <script src="https://ajax.googleapis.com/ajax/libs/jquery/1.12.4/jquery.min.js"></script>
    <script src="https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.min.js"></script> 
    <script>
getAlarm();	// load current values

function saveAlarm()
{
	var parms = "volume=" + $("#volume").val();
	parms += "&alarmSound=" + encodeURIComponent($("#alarmSound").val());

	for (var dy = 0; dy < 7; dy++)
	{
		var alarmTime = $( "#time"+dy ).val();
		if (!alarmTime)
			alarmTime = "0:0";
		parms += "&alarm=" + alarmTime + ";" + dy;
	}
	$.ajax({"url": "setAlarm?" + parms, "success":displayStatus});
}

function displayStatus(data)
{
	$("#status").html(data);
	window.setTimeout(function(){$("#status").html("")}, 2000);

	getAlarm();	// update values (in case server constrained results)
}

//window.setInterval(getAlarm, 2000);


function displayAlarm(data)
{
	var obj = JSON.parse(data);

	$("#volume").val(obj["volume"]);
	$("#alarmSound").val(obj["alarmSound"]);

	for (var dy = 0; dy < 7; dy++)
	{
		var alarmTime = pad(obj["alarmDay"][dy]["alarmHour"]) + ":" + pad(obj["alarmDay"][dy]["alarmMinute"]);

		if (alarmTime == "00:00")
			$( "#time"+dy ).val(null);
		else
			$( "#time"+dy ).val(alarmTime);
	}
}

function pad(num, sz=2){ return ('000000000' + num).substr(-sz); }

function getAlarm()
{
	$.ajax({"url": "getAlarm",
		"success": displayAlarm});
}
    </script>
  </body>
</html>)EOS"

//状态
var iEnable = false;
$.msg.openLoad();
$.Ajax.get("/protocol.csp?fname=service&opt=autoweb&function=get",function(){
$.msg.closeLoad();
	if(this.error == null){
		iEnable = parseInt($.xjson(this.responseXML,"action",true));
		$("enable").className = iEnable?"iSwitch_1":"iSwitch_1-0";
	}
});

function updata(able){
	$.msg.openLoad();
	$.Ajax.post("/protocol.csp?fname=service&opt=autoweb&function=set",function(){
		$.msg.closeLoad();
		if(this.error == null){
			iEnable = able;
		}
	},{
		action:able?1:0
	});
}

$.vsubmit("enable",function(){
	this.className = this.className == "iSwitch_1"?"iSwitch_1-0":"iSwitch_1";
	return false;
});

$.vsubmit("submit",function(){
	var eb = $("enable").className == "iSwitch_1";
	if(eb == iEnable){
		return false;
	}

	if(!eb){
		$.msg.confirm("::Service_SKIP_Tip",function(){
			updata(eb);
		});
	}
	else{
		updata(eb);
	}
	return false;
});
var iEnable = false;

$.Ajax.get("/protocol.csp?fname=net&opt=firewall_en&function=get",function(){
	if(this.error == null){
		iEnable = $.xjson(this.responseXML,"FireWall_en",true) == "1";
		$("enable").className = iEnable?"iSwitch_1":"iSwitch_1-0";
	}
});

function updata(able){
	$.msg.openLoad();
	$.Ajax.post("/protocol.csp?fname=net&opt=firewall_en&function=set",function(){
		$.msg.closeLoad();
		if(this.error == null){
			iEnable = able;
		}
	},{
		firewall_en:able?1:0
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
	updata(eb);
});

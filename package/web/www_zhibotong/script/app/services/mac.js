//获得mac地址
$.Ajax.get("/protocol.csp?fname=net&opt=clonemac&function=get", function () {
	if (this.error == null) {
		var macInfo = $.xjson(this.responseXML, "clonemac", true);
		toChange(parseInt(macInfo.macCloneEnabled));
		$("mac").value = decodeURIComponent(macInfo.macCloneMac || "");
	}
});
function toChange(flag){
	var enable = $("enable");
	if(flag){
		enable.v = 1;
		enable.className = "iSwitch_1";
		$("macInfo").style.display = "block";
	}else{
		enable.v = 0;
		enable.className = "iSwitch_1-0";
		$("macInfo").style.display = "none";
	}
}

$.dom.on($("enable"),"vclick",function(){
	if(this.v){
		toChange(false);
	}else{
		toChange(true);
	}
	
});
$.vsubmit("submit", setMac);
//设置
function setMac() {
	var mac = $("mac").value.trim(),flag = $("enable").v;
	var obj = {};
	// /protocol.csp?fname=net&opt=clonemac&function=set&macCloneEnabled=1/0&macCloneMac=xx.xx.xx.xx
	if(flag){
	  if(mac == ""){
	  	$.msg.alert("::Service_Mac_null",function(){
		   $("mac").focus();
		});
		  return ;
	  }
	  for(var i = 1; i < parseInt(mac.length / 3)+1; i++){
		if(mac[3*i-1] != ":"){
			$.msg.alert("::Service_Mac_Error",function(){
			   $("mac").focus();
			});
			return ;
		}
	  }
	  if(!/^[a-f\d]{12}$/i.test(mac.replace(/\:/g,""))){
			$.msg.alert("::Service_Mac_Format",function(){
			   $("mac").focus();
			});
			return ;
	  }
	  
	  if(parseInt(mac.split(":")[0],16) % 2 != 0){
			$.msg.alert($.lge.get("Service_Mac_First") +"“"+mac.split(":")[0]+"”"+ $.lge.get("Service_Mac_Even"),function(){
				$("mac").focus();
		});
			return ;
	  };
		obj.macCloneEnabled = 1;
		obj.macCloneMac = mac;
	 }else{
		obj.macCloneEnabled = 0;
	 }
		
		$.msg.openLoad();
	$.Ajax.post("/protocol.csp?fname=net&opt=clonemac&function=set",function(){
		$.msg.closeLoad();
		if(!this.error){
			$.msg.alert("::Service_Mac_Modify");
			$.Ajax.post("/protocol.csp?fname=net&opt=wifi_active&function=set&active=wifi_phymode",function(){
				
			});
		}
	},obj);
}
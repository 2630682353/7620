$("Metro_settings").onclick = function(){
	$.systemSetMain();
	return false;
};
if($.config.showFirmware){
		//设备系统版本号
		$.Ajax.get("/protocol.csp?fname=system&opt=devinfo&function=get",function(){
			if(this.error == null){
				$("mm_ShowFirmware").innerHTML = "FW Ver."+$.xjson(this.responseXML,"version",true);
				$("mm_ShowFirmware").style.display = "block";
			}
		});
}	
if($.config.hasHelpUrl){
	var help = $("mm_help");
	$.dom.on(help,"vclick",function(){
		//$.vhref($.config.hasHelpUrl);
		window.open($.config.hasHelpUrl);
	});
	help.setAttribute("title",$.lge.get("Help"));
	help.style.display = "block";
}
//HDD SD 显示
void function(){
	var mSD = $("mm_SD"),mHDD = $("mm_HDD");
	//if($.config.hasHDD && $.config.hasPlug){
	//	mHDD.style.display = "block";
	//}
	if($.config.hasSD && $.config.hasPlug){
		//$.hotPlug.getStorage();
		mSD.style.display = "block";
	}
	$.hotPlug.storageChange = function(storage){
		mHDD.className = storage.usbdisk1 || storage.usbdisk2 || storage.usbdisk3 ? "MMetro_bottom_DISK":"MMetro_bottom_DISK-0";
		mSD.className = storage.sdcard || storage.sdcard1 || storage.sdcard2 || storage.sdcard3 ? "MMetro_bottom_SD":"MMetro_bottom_SD-0";
	};
	//悬停和单击
	mSD.setAttribute("title",($.lge.get("Metro_StrorageIcon") || $.lge.get("IM_Storage")));
	mHDD.setAttribute("title",($.lge.get("Metro_StrorageIcon") || $.lge.get("IM_Storage")));
	$.dom.on(mSD,"vclick",function(){
		if($.data.account == "admin"){
			$.vhref("information/storage.html");
		}
		
	});
	$.dom.on(mHDD,"vclick",function(){
		if($.data.account == "admin"){
			$.vhref("information/storage.html");
		}
	});
}();

//InterNet 状态
var netIcon = $("mm_Net");
var netS;
netIcon.setAttribute("title",($.lge.get("Metro_InternetIcon") || $.lge.get("Setting_Network_Internet")));
$.dom.on(netIcon,"vclick",function(){
	if($.data.account == "admin"){
		$.vhref("network/internet.html");
	}
});
(function getInterNet(){
	$.Ajax.get("/protocol.csp?fname=net&opt=led_status&function=get",function(){
	var x = $.xjson(this.responseXML,"led_status",true);
	//使用全局配置，无需5秒请求一次dom
	if(netS != x.internet){
		netIcon.className = x.internet==0?"MMetro_bottom_Internet_ON":"MMetro_bottom_Internet_OFF";
		netIcon.style.display = "block"
	}
	netS = x.internet;
	});
	setTimeout(function(){getInterNet();},5000);
})();

$.power.appendEvent("getStart",function(){
	if($.config.hasPower){
	$("mm_Power").style.display = "";
	}else{
	$("mm_Power").style.display = "none";
	}
	
});
$.power.appendEvent("getSuccess",function(){
	//var c = $("mm_Power"),b = this.battery;
	//$("mm_Power").className = b>80?"MMetro_bottom_Power":b>25?"MMetro_bottom_Power1":"MMetro_bottom_Power2";
	//c.style.display = "block";
});
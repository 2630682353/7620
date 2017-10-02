$.setConfig(function(){
	var c = this;
	c.language = ["us","zh_CN","tr_CN","fr_FR","de_DE","sp_SP","it_IT"];
	c.title = "TripMate";
	c.img_storage = "default/storage_lan/";
	c.lge_path = "themes/HT-TM05/lge/";
	c.hasHDD = false;
	c.hasPlug = false;
	c.hasPower = false;
	c.hasHideSSID = true;
	c.hasInlayDisk = false;
	c.dlnaMultipleDir = true;
	c.hasSkip = true;
	c.hasWanAccess = false;
	c.autoLogin = function(){
		$.Ajax.post("/protocol.csp?function=set",function(){
			if(this.error){
				$.msg.alert("::20104030");
				return false;
			}
			$.lge.save();
			$.data.account = "guest";
			document.cookie = "lastaccount=guest; expires=" + new Date("2099/12/31").toGMTString() + "; path=/";
			document.cookie = "hasfirmcheck=1; path=/";
			//$.msg.openLoad();
			if(location.pathname == "/" || location.pathname == "/index.html"){
				$.systemMain();
			}
			return false;
		},{fname:"security",opt:"pwdchk",name:"admin",pwd1:""});
	};
});
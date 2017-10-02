var isCheck = getQueryString("c");
var autoAccount = getQueryString("acc");
var autoPasswd = getQueryString("pwd");
var myAccount = "";
var myPassword = "";
var isGet = false;
var interval = 0;
var checkWanDetectLinkInterval = 0;
var checkWanDetectModeInterval = 0;
var timeout = 0;
var timeoutCount = 15;
var timeoutCheck = 0;
var timeoutCheckCount = 10;
var wanConfigJsonObject;
var wanInfoJsonObject;
var deviceJsonObject;
var currentLanIP = "";
var currentMode = "";
var myMac = "";
var myStaticIp = "";
var myStaticMask = "";
var myStaticGw = "";
var myStaticDns = "";
var isConfiged = false;
var isConnected = false;
var responseReason = 0;
var userLoginStatus = false;
/*
 * 设置PPPOE账号密码
 */
function setPPPOE(account, passwd, mac, mtu, dns, dns1, clone) {
    $("#btn_1").attr("disabled", true);
    $("#btn_1").val("拨号中...");
    if (typeof (wanConfigJsonObject) == 'undefined') {
        getWanConfigJsonObject();
    }
    if (clone > 1) {
        mac = getCloneMac(clone);
    }

    dns = dns.length > 0 ? dns : "";
    dns1 = dns1.length > 0 ? dns1 : "";

    if (currentMode != 2 || mac != myMac || account != wanConfigJsonObject.user || passwd != wanConfigJsonObject.passwd || clone > 0 || dns != wanConfigJsonObject.dns || dns1 != wanConfigJsonObject.dns1 || mtu != wanConfigJsonObject.mtu || wanInfoJsonObject.connected == 0) {
        var data = "fname=net&opt=wan_conf&function=set&user=" + account + "&passwd=" + passwd + "&mode=2&mtu=" + mtu + "&dns=" + dns + "&dns1=" + dns1 + "&mac=" + mac;
        $.ajax({
            type: "POST",
            url: actionUrl + data + "&math=" + Math.random(),
            dataType: "json",
            success: function (data) {
                var jsonObject = data;
                if (jsonObject.error == 0) {
                    currentMode = 2;
                    intervalTime(1, '拨号', '宽带拨号', 2);
                } else {
                    locationUrl(jsonObject.error);
                    $("#btn_1").attr("disabled", false);
                    $("#btn_1").val("开始拨号");
                    return;
                }
            }, error: function () {
                getMsg('拨号超时,请重新连接路由器！');
                return;
            }
        });
    } else {
        if (wanInfoJsonObject.connected == 0) {
            intervalTime(1, '拨号', '宽带拨号', 2);
        } else {
            toUrl();
        }
    }
}


/*
 * 设置DHCP
 */
function setDHCP(mac, mtu, dns, dns1, clone) {
    $("#btn_3").attr("disabled", true);
    $("#btn_3").val("连接中...");
    if (typeof (wanConfigJsonObject) == 'undefined') {
        getWanConfigJsonObject();
    }
    if (clone > 1) {
        mac = getCloneMac(clone);
    }

    dns = dns.length > 0 ? dns : "";
    dns1 = dns1.length > 0 ? dns1 : "";

    if (currentMode != 1 || mac != myMac || clone > 0 || dns != wanConfigJsonObject.dns || dns1 != wanConfigJsonObject.dns1 || mtu != wanConfigJsonObject.mtu || wanInfoJsonObject.connected == 0) {
        var data = "fname=net&opt=wan_conf&function=set&mode=1&mtu=" + mtu + "&dns=" + dns + "&dns1=" + dns1 + "&mac=" + mac;
        $.ajax({
            type: "POST",
            url: actionUrl + data + "&math=" + Math.random(),
            dataType: "json",
            success: function(data) {
                var jsonObject = data;
                if (jsonObject.error == 0) {
                    currentMode = 1;
                    intervalTime(3, '启用', 'DHCP配置', 1);
                } else {
                    locationUrl(jsonObject.error);
                    $("#btn_3").attr("disabled", false);
                    $("#btn_3").val("重新启用");
                    return;
                }
            }, error: function() {
                getMsg('动态IP配置超时,请重新连接路由器！');
                return;
            }
        });
    } else {
        if (wanInfoJsonObject.connected == 0) {
            intervalTime(3, '启用', 'DHCP配置', 1);
        } else {
            toUrl();
        }
    }
}


/*
 * 设置静态IP
 */
function setStatic(ip, mask, gw, dns, dns1, mac, mtu, clone) {
    $("#btn_2").attr("disabled", true);
    $("#btn_2").val("连接中...");
    if (typeof (wanConfigJsonObject) == 'undefined') {
        getWanConfigJsonObject();
    }
    if (clone > 1) {
        mac = getCloneMac(clone);
    }

    dns = dns.length > 0 ? dns : "";
    dns1 = dns1.length > 0 ? dns1 : "";

    if (currentMode != 3 || mac != myMac || ip != myStaticIp || mask != myStaticMask || gw != myStaticGw || clone > 0 || dns != wanConfigJsonObject.dns || dns1 != wanConfigJsonObject.dns1 || mtu != wanConfigJsonObject.mtu || wanInfoJsonObject.connected == 0) {
        var data = "fname=net&opt=wan_conf&function=set&ip=" + ip + "&mask=" + mask + "&gw=" + gw + "&mode=3&mtu=" + mtu + "&dns=" + dns + "&dns1=" + dns1 + "&mac=" + mac;
        $.ajax({
            type: "POST",
            url: actionUrl + data + "&math=" + Math.random(),
            dataType: "json",
            success: function(data) {
                var jsonObject = data;
                if (jsonObject.error == 0) {
                    currentMode = 3;
                    intervalTime(2, '确认', '静态IP设置', 3);
                } else {
                    locationUrl(jsonObject.error);
                    $("#btn_2").attr("disabled", false);
                    $("#btn_2").val("重新确认");
                    return;
                }
            }, error: function() {
                getMsg('静态IP配置超时,请重新连接路由器！');
                return;
            }
        });
    } else {
        if (wanInfoJsonObject.connected == 0) {
            intervalTime(2, '确认', '静态IP设置', 3);
        } else {
            toUrl();
        }
    }
}

/*
 * 无线中继
 */
function getWifiList() {
    if (typeof (wanInfoJsonObject) == 'undefined') {
        getWanInfoJsonObject();
    }

    if (wanInfoJsonObject.connected == 1 && wanInfoJsonObject.mode == 4) {
        $("#wf_name").text(wanConfigJsonObject.ssid);
        $("#noinpt").text("已连接");
    } else {
        $("#wf_name").text('请选择');
    }
    loading(1, '正在加载...');
    $.ajax({
        type: 'POST',
        url: actionUrl + "fname=net&opt=ap_list&function=get&math=" + Math.random(),
        dataType: "JSON",
        success: function(data) {
            var jsonWifiListObject = data;
            if (jsonWifiListObject.error == 0) {
                var aplist = jsonWifiListObject.aplist;
                var wifi = "";
                var list = "";
                var data_ssid = new Array();
                if (aplist.length > 0) {
                    for (var i = 0; i < aplist.length; i++) {
                        if (aplist[i].dbm >= 0 && aplist[i].dbm <= 33) {
                            aplist[i].dbm = '"lvl lvl-3"';
                        } else if (aplist[i].dbm >= 34 && aplist[i].dbm <= 66) {
                            aplist[i].dbm = '"lvl lvl-2"';
                        } else if (aplist[i].dbm >= 67 && aplist[i].dbm <= 100) {
                            aplist[i].dbm = '"lvl lvl-1"';
                        }
                        if (aplist[i].security == 'NONE') {
                            aplist[i].security_lock = "<div class=" + aplist[i].dbm + "></div>";
                        } else {
                            aplist[i].security_lock = "<div class=" + aplist[i].dbm + "><i class='lock'></i></div>";
                        }
                        if (aplist[i].ssid.length > 15) {
                            data_ssid[i] = aplist[i].ssid.substr(0, 15) + '...';
                        } else {
                            data_ssid[i] = aplist[i].ssid;
                        }
                        if (aplist[i].ssid.length > 1) {
                            list = "<tr style='cursor: pointer' ssid='" + aplist[i].ssid + "' channel=" + aplist[i].channel + " sec=" + aplist[i].security + "><td width='143' title='" + aplist[i].ssid + "'>" + data_ssid[i] + "</td><td width='170'>" + aplist[i].bssid.toUpperCase() + "</td><td width='40'>" + aplist[i].channel + "</td><td width='230'>" + aplist[i].security + "</td><td>" + aplist[i].security_lock + "</td></tr>";
                            wifi += list;
                        }
                    }
                }
                layer.closeAll();
                $("#getWifiList").empty().html(wifi);
            } else {
                layer.closeAll();
                locationUrl(data.error);
                return;
            }
        }, complete: function (XHR, TS) {
            XHR = null;
        }, error: function (XHRequest, status, data) {
            XHRequest.abort();

        }
    });
}
//获取附近无线网
function WifiList() {
    loading(1, '正在加载...');
    $.ajax({
        type: 'POST',
        url: actionUrl + "fname=net&opt=ap_list_5g&function=get&math=" + Math.random(),
        dataType: "JSON",
        success: function (data) {
            var jsonWifiListObject = data;
            if (jsonWifiListObject.error == 0) {
                var aplist = jsonWifiListObject.aplist;
                var wifi = "";
                var list = "";
                var data_ssid = new Array();
                if (aplist.length > 0) {
                    for (var i = 0; i < aplist.length; i++) {
                        if (aplist[i].dbm >= 0 && aplist[i].dbm <= 33) {
                            aplist[i].dbm = '"lvl lvl-3"';
                        } else if (aplist[i].dbm >= 34 && aplist[i].dbm <= 66) {
                            aplist[i].dbm = '"lvl lvl-2"';
                        } else if (aplist[i].dbm >= 67 && aplist[i].dbm <= 100) {
                            aplist[i].dbm = '"lvl lvl-1"';
                        }
                        if (aplist[i].security == 'NONE') {
                            aplist[i].security_lock = "<div class=" + aplist[i].dbm + "></div>";
                        } else {
                            aplist[i].security_lock = "<div class=" + aplist[i].dbm + "><i class='lock'></i></div>";
                        }
                        if (aplist[i].ssid.length > 15) {
                            data_ssid[i] = aplist[i].ssid.substr(0, 15) + '...';
                        } else {
                            data_ssid[i] = aplist[i].ssid;
                        }
                        if (aplist[i].ssid.length > 1) {
                            list = "<tr><td width='70'><input channel=" + aplist[i].channel + " sec=" + aplist[i].security + " type='radio' name='wifiRadio' value=" + aplist[i].ssid + "></td><td width='143' title='" + aplist[i].ssid + "'>" + data_ssid[i] + "</td><td width='170'>" + aplist[i].bssid.toUpperCase() + "</td><td width='40'>" + aplist[i].channel + "</td><td width='230'>" + aplist[i].security + "</td><td>" + aplist[i].security_lock + "</td></tr>";
                            wifi += list;
                        }
                    }
                }
                $("#getWifiList").append(wifi);
            }
        }, complete: function (XHR, TS) {
            XHR = null;
        }, error: function (XHRequest, status, data) {
            XHRequest.abort();
            
            
        }
    });
    layer.closeAll();
}
/*
 * 无线中继连接
 * @param {type} ssid
 * @param {type} pwd
 * @param {type} mac
 * @param {type} channel
 */
function connectWisp(ssid, pwd, mac, channel, sec) {
    $("#btn_4").attr("disabled", true);
    $("#btn_4").val("连接中...");
    var dat, dat2;

    if (typeof (wanInfoJsonObject) == 'undefined') {
        getWanInfoJsonObject();
    }

    if (typeof (wanConfigJsonObject.ssid) == 'undefined') {
        getWanConfigJsonObject();
    }

    if (sec == 1) {
        sec = 'NONE';
    } else if (sec != 'NONE') {
        var secsp = sec.split('/');
        if (secsp.length == 1) {
            sec = sec.replace(' ', '') + '/TKIPAES';
        }
    }

    if (pwd == '') {
        pwd = 'NONE';
    }

    if (mac == '00:00:00:00:00:00') {
        mac = wanConfigJsonObject.rawmac;
    }

    dat = "fname=net&opt=wan_conf&function=set" + "&mac=" + mac + "&mode=4&ssid=" + ssid + "&security=" + sec + "&key=" + pwd + "&channel=" + channel;
    dat2 = "fname=net&opt=wan_conf&function=get" + "&mac=" + mac + "&mode=4&ssid=" + ssid + "&security=" + sec + "&key=" + pwd + "&channel=" + channel;
    if (currentMode != 4 || mac != myMac || wanInfoJsonObject.connected == 0 || wanConfigJsonObject.ssid != ssid) {
        $.ajax({
            type: "POST",
            url: actionUrl + dat + "&math=" + Math.random(),
            dataType: "JSON",
            success: function (data) {
                if (data.error == 0) {
                    currentMode = 4;
                    loading(1, '正在连接...');
                    $.ajax({
                        type: "POST",
                        url: actionUrl + dat2,
                        dataType: "JSON",
                        success: function (jsonWisp) {
                            if (jsonWisp.error == 0) {
                                intervalTime(4, '中继', '中继', jsonWisp.mode);
                            } else {
                                getMsg('连接超时！');
                                $("#noinpt").text('中继失败');
                                layer.closeAll();
                            }
                        }
                    });
                } else if (data.error != 0) {
                    locationUrl(data.error);
                }
            }, complete: function (XHR, TS) {
                XHR = null;
            }, error: function (XHRequest, status, data) {
                XHRequest.abort();

            }
        });
    } else {
        if (wanInfoJsonObject.connected == 0) {
            intervalTime(4, '中继', '中继', 4);
        } else if (wanInfoJsonObject.connected == 1 && wanInfoJsonObject.mode == 4) {
            getMsg("当前WIFI已中继成功！");
            setTimeout(function () {
                toUrl();
            }, 3000);
        }
    }
}





function getWanConfigJsonObject() {
    $.ajax({
        type: "POST",
        url: actionUrl + "fname=net&opt=wan_conf&function=get&math=" + Math.random(),
        dataType: "json",
        async: false,
        success: function (data) {
            var jsonObject = data;
            if (jsonObject.error == 0) {
                wanConfigJsonObject = jsonObject;
            } else {
                locationUrl(data.error);
                return;
            }
        }, complete: function (XHR, TS) {
            XHR = null;
        },
        error: function (XHRequest, status, data) {
            getMsg('请重新连接路由器！');
            layer.closeAll();
            XHRequest.abort();
        }
    });
}

function getDeviceCheck() {
    $.ajax({
        type: "POST",
        url: actionUrl + "fname=system&opt=device_check&function=get&math=" + Math.random(),
        async: false,
        dataType: "json",
        success: function (data) {
            if (data.error == 0) {
                deviceJsonObject = data;
            } else {
                locationUrl(data.error);
                return;
            }
        }
    });
}

/*
 * 获取wanInfoJson
 */
function getWanInfoJsonObject() {
    $.ajax({
        type: "POST",
        url: actionUrl + "fname=net&opt=wan_info&function=get&math=" + Math.random(),
        async: false,
        dataType: "json",
        success: function (data) {
            var jsonObject = data;
            if (jsonObject.error == 0) {
                wanInfoJsonObject = jsonObject;
                currentMode = wanInfoJsonObject.mode;
                if (jsonObject.reason > 0) {
                    responseReason = jsonObject.reason;
                }
            } else {
                getMsg(getErrorCode(jsonObject.error));
                locationUrl(data.error);
                return;
            }
        }, complete: function (XHR, TS) {
            XHR = null;
        },
        error: function (XHRequest, status, data) {
            getMsg('请重新连接路由器！');
            layer.closeAll();
            XHRequest.abort();
        }
    });
}

/*
 * 检查网线
 */
function checkWanDetectLink() {
    $.ajax({
        type: "POST",
        url: actionUrl + "fname=net&opt=wan_detect&function=get&math=" + Math.random(),
        dataType: "json",
        success: function (data) {
            var jsonObject = data;
            if (jsonObject.wan_link == 1) {
                clearInterval(checkWanDetectLinkInterval);
                checkWanDetect(0);
            } else if (jsonObject.wan_link == 0) {
                getWanInfoJsonObject();
                if (wanInfoJsonObject.connected == 1) {
                    toUrl();
                } else {
                    layer.closeAll();
                    clearInterval(checkWanDetectLinkInterval);
                    var lineDivShow = $("#lineOff");
                    loading(1, lineDivShow, 1);
                }
            }
        }
    });
}

/*
 * 检查网络类型
 */
function checkWanDetect(type) {
    $.ajax({
        type: "POST",
        url: actionUrl + "fname=net&opt=wan_detect&function=get&math=" + Math.random(),
        dataType: "json",
        success: function(data) {
            var jsonObject = data;
            if (jsonObject.error == 0) {
                if (jsonObject.wan_link == 0 && type < 1) {
                    checkWanDetectLinkInterval = setInterval(function() {
                        checkWanDetectLink();
                    }, 2000);
                } else {
                    layer.closeAll();
                    clearInterval(checkWanDetectLinkInterval);
                    if (jsonObject.connected == 0) {
                        if (type > 0) {
                            netTypeChoice(type);
                        } else {
                            netTypeChoice(jsonObject.mode);
                        }
                    } else {
                        toUrl();
                    }
                }
            } else {
                locationUrl(data.error);
                return;
            }
        }
    });
}


function userLogin(name, pwd) {
    var str_md5 = $.md5(name + pwd);
    $.ajax({
        type: "POST",
        url: actionUrl + "fname=system&opt=login&function=set&usrid=" + str_md5 + "&math=" + Math.random(),
        dataType: "JSON",
        success: function (data) {
            if (data.error == 0) {
                $.cookie('lstatus', true);
                document.location = 'quick.html?math=' + Math.random();
            } else if (data.error == 10001) {
                getMsg('用户名或密码错误！');
                $("#user_login").text('重新登录');
                $("#user_login").attr("disabled", false);
            } else {
                getMsg('登录失败');
                $("#user_login").text('重新登录');
                $("#user_login").attr("disabled", false);
            }
        }, error: function () {
            $("#user_login").text('重新登录');
            $("#user_login").attr("disabled", false);
            getMsg('连接超时,请刷新页面！');
        }
    });
}

function setLedOnOff(act, val) {
    var dat = "fname=system&opt=led&function=get&act=dump";
    if (act == 'set') {
        dat = "fname=system&opt=led&function=set&act=" + val;
    }
    $.ajax({
        type: "POST",
        url: actionUrl + dat + "&math=" + Math.random(),
        dataType: "JSON",
        success: function(data) {
            if (data.error == 0) {
                if (act == 'set') {
                    getMsg('设置成功！');
                } else {
                    if (data.led == 1) {
                        if ($("#led_on").hasClass('selected') == false) {
                            $("#led_on").addClass('selected');
                        }
                        if ($("#led_off").hasClass('selected') == true) {
                            $("#led_off").removeClass('selected');
                        }
                    } else {
                        if ($("#led_on").hasClass('selected') == true) {
                            $("#led_on").removeClass('selected');
                        }
                        if ($("#led_off").hasClass('selected') == false) {
                            $("#led_off").addClass('selected');
                        }
                    }
                }
            } else {
                locationUrl(data.error);
            }
        }
    });
}

function wifiGet(type) {
    $.ajax({
        type: "POST",
        url: actionUrl + "fname=&opt=wifi_lt" + type + "&function=get",
        dataType: "JSON",
        success: function (data) {
            if (data.error == 0) {
                router.getWifiAp();
                $("#startHour" + type + " option[value=" + data.sh + "]").attr('selected', 'selected');
                $("#startMin" + type + " option[value=" + data.sm + "]").attr('selected', 'selected');
                $("#endHour" + type + " option[value=" + data.eh + "]").attr('selected', 'selected');
                $("#endMin" + type + " option[value=" + data.em + "]").attr('selected', 'selected');
                $("input[name=day" + type + "]").each(function (index) {
                    for (var i = 0; i <= data.week.length; i++) {
                        if (index == data.week[i]) {
                            $(this).attr("checked", true);
                        }
                    }
                });
                if (data.time_on == 1) {
                    $("#wifiTimeOn" + type).addClass('selected');
                    $("#wifiTimeOff" + type).removeClass('selected');
                    $("#wf_time_div" + type).show();
                } else if (data.time_on == 0) {
                    $("#wifiTimeOff" + type).addClass('selected');
                    $("#wifiTimeOn" + type).removeClass('selected');
                    $("#wf_time_div" + type).hide();
                }

                if (data.enable == 1) {
                    $("#wifi_on" + type).addClass('selected');
                    $("#wifi_off" + type).removeClass('selected');
                    $("#wifisetpanel" + type).show();
                    if (type != '_5g') {
                        $("#rate").show();
                        $("#guest").show();
                        $("#panel").show();
                    } else if (type == '_5g') {
                        $("#panel" + type).show();
                        $("#rate").show();
                    }
                } else if (data.enable == 0) {
                    $("#wifi_off" + type).addClass('selected');
                    $("#wifi_on" + type).removeClass('selected');
                    $("#wifisetpanel" + type).hide();
                    if (type != '_5g') {
                        $("#rate").hide();
                        $("#guest").hide();
                        $("#panel").hide();
                    } else if (type == '_5g') {
                        $("#panel" + type).hide();
                    }
                }
            } else {
                locationUrl(data.error);
            }
        }
    });
}


function wifiSet(type, enable, time_on, week, sh, sm, eh, em) {
    $.ajax({
        type: "POST",
        url: actionUrl + "fname=net&opt=wifi_lt" + type + "&function=set&enable=" + enable + "&time_on=" + time_on + "&week=" + week + "&sh=" + sh + "&sm=" + sm + "&eh=" + eh + "&em=" + em,
        dataType: "JSON",
        success: function (data) {
            if (data.error == 0) {
                if (enable == 0 && type != '_5g') {
                    $("#wifisetpanel").hide();
                    $("#guest").hide();
                    $("#panel").hide();
                    if ($("#wifi_off_5g").hasClass('selected')) {
                        $("#rate").hide();
                    }
                } else if (enable == 0 && type == '_5g') {
                    $("#wifisetpanel" + type).hide();
                    $("#panel" + type).hide();
                    if ($("#wifi_off").hasClass('selected')) {
                        $("#rate").hide();
                    }
                } else if (enable == 1) {
                    $("#wifisetpanel" + type).show();
                    if (type == '') {
                        $("#guest").show();
                    }
                    $("#panel" + type).show();
                    $("#rate").show();
                }
                getMsg('设置成功！');
            } else {
                locationUrl(data.error);
            }
        }
    });
}

function wifiTxrateGet(act, rate) {
    var dat = "fname=net&opt=txrate&function=" + act;
    var dat5g = "fname=net&opt=txrate_5g&function=set&txrate=" + rate;
    if (rate > 0) {
        dat = "fname=net&opt=txrate&function=" + act + "&txrate=" + rate;
    }
    $.ajax({
        type: "POST",
        url: actionUrl + dat,
        dataType: "JSON",
        success: function (data) {
            if (data.error == 0) {
                if (rate > 0) {
//                    getMsg('设置成功！');
                    $.ajax({
                        type: "POST",
                        url: actionUrl + dat5g,
                        dataType: "JSON",
                        success: function (data5g) {
                            if (data5g.error == 0) {
                                getMsg('设置成功！');
                            } else {
                                rate = oldrate;
                                $.ajax({
                                    type: "POST",
                                    url: actionUrl + dat,
                                    dataType: "JSON",
                                    success: function (databack) {
                                        if (databack.error != 0) {
                                            locationUrl(data.error);
                                        }
                                    }
                                });
                                locationUrl(data.error);
                            }
                        }
                    });
                } else {
                    oldrate = data.txrate;
                    if (data.txrate <= 30) {
                        $("#rate_mode_1").addClass('selected');
                        $("#rate_mode_2").removeClass('selected');
                        $("#rate_mode_3").removeClass('selected');
                    } else if (data.txrate <= 65 && data.txrate > 30) {
                        $("#rate_mode_2").addClass('selected');
                        $("#rate_mode_1").removeClass('selected');
                        $("#rate_mode_3").removeClass('selected');
                    } else if (data.txrate <= 100 && data.txrate > 65) {
                        $("#rate_mode_3").addClass('selected');
                        $("#rate_mode_1").removeClass('selected');
                        $("#rate_mode_2").removeClass('selected');
                    }
                }
            } else {
                locationUrl(data.error);
            }
        }
    });
}
/*
 * wifi来宾模式设置
 * @param {type} act
 * @param {type} status
 */
function wifiGuestGet(act, status) {
    var dat = "fname=net&opt=wifi_vap&function=get";
    if (act == 'set') {
        dat = "fname=net&opt=wifi_vap&function=set" + "&enable=" + status;
    }
    $.ajax({
        type: "POST",
        url: actionUrl + dat,
        dataType: "JSON",
        success: function(data) {
            if (data.error == 0) {
                if (act == 'get' && data.enable == 1) {
                    $("#guest_on").addClass('selected');
                    $("#guest_off").removeClass('selected');
                    wifiGuestList('get');
                } else if (act == 'get' && data.enable == 0) {
                    $("#guest_off").addClass('selected');
                    $("#guest_on").removeClass('selected');
                    $("#visit_mode_div").hide();
                    $("#reGuestList").hide();
                } else {
                    getMsg('设置成功！');
                    if (act == 'set' && status == 1) {
                        wifiGuestList('get');
                        $("#reGuestList").show();
                    } else if (act == 'set' && status == 0) {
                        $("#reGuestList").hide();
                    }
                }
                if (status == 0) {
                    $("#visit_mode_div").hide();
                }
            } else {
                locationUrl(data.error);
            }
        }
    });
}

function get5GWifiAp() {
    $.ajax({
        type: "POST",
        url: actionUrl + 'fname=net&opt=wifi_ap_5g&function=get',
        dataType: 'JSON',
        success: function (data) {
            var jsonObject = data;
            if (jsonObject.error == 0) {
                $("#channel_5g").val(jsonObject.channel);
                if (jsonObject.hidden == 1) {
                    $("#hidden_ssid_5g").attr("checked", true);
                }
                $('#wf_nameset_5g').val(jsonObject.ssid).siblings("label").hide();
                $('#wf_pwdset_5g').val(jsonObject.passwd).siblings("label").hide();
                $("#wifiChannel_5g option[value=" + jsonObject.channel + "]").attr('selected', 'selected');
                $("#wifiBandwidth_5g option[value=" + jsonObject.bw + "]").attr('selected', 'selected');
            } else {
                locationUrl(data.error);
            }
        }
    });
}

/*
 *端口映射 
 */
function hostNatList(fc, mac, act, out_port, in_port, proto, enable, tr) {
    var dat = "fname=net&opt=host_nat&function=" + fc + "&mac=" + mac + "&act=" + act;
    if (act == 'add' || act == 'del' || act == 'mod') {
        dat = "fname=net&opt=host_nat&function=" + fc + "&mac=" + mac + "&act=" + act + "&out_port=" + out_port + "&in_port=" + in_port + "&proto=" + proto + "&enable=" + enable;
    }
    $.ajax({
        type: "POST",
        url: actionUrl + dat,
        dataType: "JSON",
        success: function(data) {
            if (data.error == 0) {
                var html = '';
                if (fc == 'get') {
                    for (var i = 0; i < data.nat.length; i++) {
                        data.nat[i].mark = switch_open(data.nat[i].enable, 2);
                        data.nat[i].protoMark = changeTcpUdp(data.nat[i].proto);
                        html += "<tr><td>" + data.nat[i].out_port + "</td><td>" + data.nat[i].in_port + "</td><td>" + data.nat[i].protoMark + "</td><td class='mod_nat'><div class='tbDiv'><i class='" + data.nat[i].mark + "'></i></div></td><td class='del_nat'><div class='editBtn remove'></div></td></tr>";
                    }
                    $("#host_nat").empty().html(html);
                } else if (fc == 'set') {
                    if (act == 'del') {
                        getMsg('删除成功！');
                        $("#host_nat").find('tr').eq(tr).remove();
                    } else if (act == 'add') {
                        getMsg('添加成功！');
                        var mark = switch_open(enable, 2);
                        proto = changeTcpUdp(proto);
                        $("#dk_out").val('');
                        $("#dk_inner").val('');
                        html = "<tr><td>" + out_port + "</td><td>" + in_port + "</td><td>" + proto + "</td><td class='mod_nat'><div class='tbDiv'><i class='" + mark + "'></i></div></td><td class='del_nat'><div class='editBtn remove'></div></td></tr>";
                        $("#host_nat").append(html);
                    } else if (act == 'mod') {
                        getMsg('修改成功！');
                    }
                }
            } else {
                locationUrl(data.error);
            }
        }
    });
}

function routerRestart(act) {
    $.ajax({
        type: "POST",
        url: actionUrl + "fname=system&opt=setting&action=" + act + "&function=set",
        dataType: "JSON",
        success: function(data) {
            if (data.error == 0) {
                var content = '';
                if (act == 'reboot') {
                    content = "路由器正在重新启动...";
                } else {
                    content = "路由器正在恢复出厂设置...";
                }
                GetProgressBar(content);
            } else {
                locationUrl(data.error);
            }
        }
    });
}

function checkConfig() {
    if ($.cookie('lstatus') == 'true') {
        loading(1, '正在检测上网方式...');
        getWanConfigJsonObject();
        myMac = (wanConfigJsonObject.mac).toUpperCase();
        $("#mac_inpt").val(myMac).siblings("label").hide();
        currentMode = wanConfigJsonObject.mode;
        //获取账号成功后跳转至此
        if (autoAccount != '' && autoAccount != null) {
            $("#acc").val(autoAccount).siblings("label").hide();
            $("#pwd").val(autoPasswd).siblings("label").hide();
            $("#netWorkSet").show().siblings(".wrap").hide();
            $("#t_1").addClass('selected');
            $("#t_1_box").show();
            layer.closeAll();
            return;
        } else {
            getWanInfoJsonObject();
            getDeviceCheck();
            if (deviceJsonObject.config_status == 0) {	//未配置
                isConfiged = false;
                checkWanDetect(0);
            } else {  //已配置
                isConfiged = true;
                if (wanInfoJsonObject.link == 0) {
                    checkWanDetectLinkInterval = setInterval(function () {
                        checkWanDetectLink();
                    }, 2000);
                } else if (wanInfoJsonObject.connected == 0) {
                    isConnected = false;
                    layer.closeAll();
                    netTypeChoice(wanConfigJsonObject.mode);
                } else {
                    isConnected = true;
                    toUrl();
                }
            }
        }
    } else {
        $("#account_login").show();
    }
}

/*
 * 设置
 */

function checkSetting() {
    userLoginStatus = $.cookie('lstatus');
    if (userLoginStatus == 'true') {
        loading(1, '正在检测上网方式...');
        getWanConfigJsonObject();
        getWanInfoJsonObject();
        myMac = (wanConfigJsonObject.mac).toUpperCase();
        $("#mac_inpt").val(myMac).siblings("label").hide();
        $("#macInpt").text(myMac);
        layer.closeAll();
        netTypeChoice(wanConfigJsonObject.mode);
    } else {
        locationUrl(10007);
    }
}

/*
 * 获取宽带账号
 */
function getPPPOEAccount() {
    $(".dot").hide();
    $(".txt").removeClass("hide").text("路由器正在获取中...");
    $(".d2").removeClass("error");
    $('#getAccount').val("获取中...");
    $('#getAccount').attr("disabled", true);
    timeoutCount = 30;
    $.ajax({
        type: "POST",
        url: actionUrl + "fname=net&opt=wan_account&function=set",
        dataType: "json",
        success: function (data) {
            var jsonObject = data;
            if (jsonObject.error == 0) {
                $.ajax({
                    type: "POST",
                    url: actionUrl + "fname=net&opt=wan_account&function=get",
                    dataType: "json",
                    success: function (data) {
                        var jsonObject = data;
                        if (jsonObject.error == 0) {
                            layer.closeAll();
                            loading(1, $('#sucLayer'), 1);
                            $('#account_get').empty().text(jsonObject.account);
                            $('#pwd_get').empty().text(jsonObject.passwd);
                        } else {
                            interval = setInterval(function () {
                                if (timeout > timeoutCount - 1) {
                                    timeout = 0;
                                    $(".dot").show();
                                    $(".d2").addClass("error");
                                    $(".txt").text("获取失败!");
                                    clearInterval(interval);
                                    $("#getAccount").removeAttr("disabled");
                                    $("#getAccount").val("重新获取");
                                }
                                if (!isGet) {
                                    $(".d1").delay(100).fadeIn();
                                    $(".d2").delay(400).fadeIn();
                                    $(".d3").delay(700).fadeIn();
                                    if (timeout != 0) {
                                        setTimeout(function () {
                                            $(".dot").fadeOut();
                                        }, 1200);
                                    }
                                    getAccountTimer();
                                    timeout++;
                                }
                            }, 2000);
                        }
                    }
                });
            } else {
                locationUrl(data.error);
            }
        }
    });
}

function getAccountTimer() {
    $.ajax({
        type: "POST",
        url: actionUrl + "fname=net&opt=wan_account&function=get",
        dataType: "json",
        success: function (data) {
            var jsonObject = data;
            if (jsonObject.error == 0) {
                clearInterval(interval);
                isGet = true;
                timeout = 0;
                layer.closeAll();
                loading(1, $('#sucLayer'), 1);
                $('#account_get').empty().text(jsonObject.account);
                $('#pwd_get').empty().text(jsonObject.passwd);
            }
        }
    });
}

function intervalTime(btn, btnText, msgText, mode) {
    responseReason = 0;
    interval = setInterval(function () {
        getWanInfoJsonObject();
        if ((wanInfoJsonObject.link == 0 && mode != 4) || responseReason == 19) {
            timeout = 15;
        }
        if (timeout > timeoutCount - 1) {
            if (wanInfoJsonObject.error == 0) {
                if (wanInfoJsonObject.connected == 0) {
                    if (wanInfoJsonObject.link == 0 && mode != 4) {
                        getMsg("请插入网线！");
                    } else {
                        if (responseReason > 0) {
                            getMsg(getErrorCode(responseReason));
                        } else {
                            getMsg(msgText + '超时！');
                        }
                    }
                } else if (wanInfoJsonObject.connected == 1 && wanInfoJsonObject.mode == mode) {
                    toUrl();
                }
            } else {
                getMsg(getErrorCode(wanInfoJsonObject.error));
            }
            if (mode == 4) {
                layer.closeAll();
                getMsg(getErrorCode(wanInfoJsonObject.error));
            }
            $("#btn_" + btn).attr("disabled", false);
            $("#btn_" + btn).val("重新" + btnText);
            timeout = 0;
            clearInterval(interval);
            return;
        }
        if (wanInfoJsonObject.connected == 0 || wanInfoJsonObject.mode != mode) {
            timeout++;
        } else if (wanInfoJsonObject.connected == 1 && wanInfoJsonObject.mode == mode) {
            toUrl();
        }
    }, 4000);
}

function checkVersion() {
    $.ajax({
        type: "POST",
        url: actionUrl + "fname=system&opt=firmstatus&function=get&math=" + Math.random(),
        dataType: "JSON",
        async: false,
        success: function (data) {
            var jsonObject = data;
            if (jsonObject.error == 0) {
                if (jsonObject.status == 0) {
                    $("#update_1").val('最新固件');
                    $("#checkMsg").text('已是最新版本');
                } else if (jsonObject.status == 1) {
                    $("#update_1").val('下载固件');
                    $("#update_1").attr('onclick', 'getGradeStatus()');
                    $("#checkMsg").text('');
                } else if (jsonObject.status == 2) {
                    $("#update_1").val('正在下载');
                    $("#update_1").attr('disabled', true);
                    $("#checkMsg").text('');
                } else if (jsonObject.status == 3) {
                    $("#update_1").attr('disabled', false);
                    $("#update_1").val('升级固件');
                    $("#update_1").attr('onclick', 'up_Wrap()');
                    $("#checkMsg").text('');
                } else if (jsonObject.status == 4) {
                    $("#update_1").val('重新升级');
                    $("#update_1").attr('onclick', 'up_Wrap()');
                }
            } else {
                locationUrl(data.error);
            }
        }, complete: function (XHR, TS) {
            XHR = null;
        }, error: function (XHRequest, status, data) {
            XHRequest.abort();
            
            
        }
    });
}

function getGradeStatus() {
    $("#update_1").val('正在下载');
    $("#update_1").attr('disabled', true);
    $.ajax({
        type: "POST",
        url: actionUrl + "fname=system&opt=firmstatus&function=set&flag=get",
        dataType: "JSON",
        success: function (doStatus) {
            if (doStatus.error == 0) {
                layer.open({
                    type: 1,
                    title: false,
                    shade: [0.7, '#000'],
                    closeBtn: false,
                    content: $("#uploadLay"),
                    skin: 'cy-class'
                });
                var ProgressBar = {
                    maxValue: 100,
                    value: 0,
                    SetValue: function (aValue) {
                        this.value = aValue;
                        if (this.value >= this.maxValue)
                            this.value = this.maxValue;
                        if (this.value <= 0)
                            this.value = 0;
                        var mWidth = this.value / this.maxValue * $("#progressBar_do").width() + "px";
                        $("#progressBar_Track").css("width", mWidth);
                        $("#progressBarTxt").html(this.value + "%");
                    }
                }
//                        设置最大值
                ProgressBar.maxValue = 100;
//                        设置当前刻度
                var index = 0;
                var mProgressTimer = window.setInterval(function () {
                    $.ajax({
                        type: "POST",
                        url: actionUrl + "fname=system&opt=firmstatus&function=get",
                        dataType: "JSON",
                        success: function (jsonDoad) {
                            if (jsonDoad.error == 0) {
                                index = ((jsonDoad.curl / jsonDoad.total) * 100).toFixed(2);
                                ProgressBar.SetValue(index);
                                if (index == 100) {
                                    window.clearInterval(mProgressTimer);
                                    ProgressBar.SetValue(0);
                                    layer.closeAll();
                                    $("#update_1").val('升级固件');
                                    $("#update_1").attr('disabled', false);
                                    $("#update_1").attr('onclick', 'up_Wrap()');
                                }
                            }
                        }
                    });
                }, 1000);
            }
        }
    });
}

function up_Wrap() {
    $.ajax({
        type: "POST",
        url: actionUrl + "fname=system&opt=firmup&function=set",
        dataType: "JSON",
        success: function (data) {
            var jsonObject = data;
            if (jsonObject.error == 0) {
                $("#update_1").val("正在升级...");
                $("#update_1").attr('disabled', true);
//                $("#getMsg").show().text("");
                var content = '正在升级，可能需要两分钟。升级过程中请勿操作或断电,升级完成后请重新登录!';
                GetProgressBar(content);
//                var sec = 180;
//                setInterval(function() {
//                    sec--;
//                    if (sec == 0) {
//                        toUrl();
//                    }
//                }, 1000);
            } else {
                locationUrl(jsonObject.error);
            }
        }
    });
}


function setUpgrade() {
    $.ajax({
        type: "POST",
        url: actionUrl + "fname=system&opt=local_firmup&function=set",
        async: false,
        dataType: "json",
        success: function (data) {
            var jsonObject = data;
            if (jsonObject.error == 0) {
                document.getElementById("_submit").value = "升级中...";
                document.getElementById("_submit").disabled = true;
                document.getElementById("msg").innerHTML = "正在升级，大约需要3分钟，请稍候...";
                initBar();
            } else {
                document.getElementById("_submit").value = "升级出错";
                document.getElementById("_submit").disabled = true;
                document.getElementById("msg").innerHTML = "升级遇到错误，请联系客服！错误码：" + jsonObject.error;
            }
        }
    });
}


function uploadFile() {
    /*var ifr = document.createElement("iframe");
     ifr.setAttribute("id", "hiddenFrame");
     ifr.setAttribute("name", "hiddenFrame");
     ifr.setAttribute("width",0);
     ifr.setAttribute("height",0);
     document.body.appendChild(ifr);*/

    if (document.getElementById("file").value == "") {
        document.getElementById("msg").innerHTML = "请先选择升级文件！";
    } else {
        if (document.getElementById("file").value.indexOf("firmware.bin") == -1) {
            document.getElementById("msg").innerHTML = "固件文件名错误！应为：firmware.bin";
        } else {
            var status = upReady();
            if (status == 0) {
                stopAuto();
                document.getElementById("_submit").value = "上传中...";
                document.getElementById("_submit").disabled = true;
                document.getElementById("msg").innerHTML = "正在上传，请稍候...";
                document.getElementById("form1").action = "/upload.csp?uploadpath=/tmp/ioos&t=firmware.bin";
                document.getElementById("form1").submit();
                interval = setInterval(function () {
                    if (timeout > timeoutCount - 1) {
                        timeout = 0;
                        clearInterval(interval);
                        document.getElementById("msg").innerHTML = "超时，上传失败！";
                        document.getElementById("_submit").value = "确认上传";
                        document.getElementById("_submit").disabled = false;
                    }
                    if (!isGet) {
                        getResponseTimer();
                        timeout++;
                    }
                }, 4000);
            } else {
                locationUrl(status);
            }
        }
    }
}

function upReady() {
    var status = 0;
    $.ajax({
        type: "POST",
        url: actionUrl + "fname=system&opt=up_ready&function=set",
        async: false,
        dataType: "json",
        success: function (data) {
            var jsonObject = data;
            if (jsonObject.error == 0) {
                status = 0;
            } else {
                status = jsonObject.error;
            }
        }
    });
    return status;
}

function getResponseTimer() {
    $.ajax({
        type: "POST",
        url: actionUrl + "fname=net&opt=upload_status&function=get",
        dataType: "json",
        success: function (data) {
            var jsonObject = data;
            if (jsonObject.error == 0) {
                if (jsonObject.status == 1) {
                    clearInterval(interval);
                    isGet = true;
                    timeout = 0;
                    setUpgrade();
                }
            } else {
                locationUrl(data.error);
            }
        }
    });
}

var barWidth = 300;
var barTimer = 500;
if (syj == null)
    var syj = {};
syj.ProgressBar = function (parent, width, barClass, display) {
    this.parent = parent;
    this.pixels = width;
    this.parent.innerHTML = "<div/>";
    this.outerDIV = this.parent.childNodes[0];
    this.outerDIV.innerHTML = "<div/>";
    this.fillDIV = this.outerDIV.childNodes[0];
    this.fillDIV.innerHTML = "0";
    this.fillDIV.style.width = "0px";
    this.outerDIV.className = barClass;
    this.outerDIV.style.width = (width + 2) + "px";
    this.parent.style.display = display == false ? 'none' : 'block';
}

syj.ProgressBar.prototype.setPercent = function (pct) {
    var fillPixels;
    if (pct < 0.99) {
        fillPixels = this.pixels * pct;
    } else {
        pct = 1.0;
        fillPixels = this.pixels;
        stopAuto();
        document.getElementById("_submit").value = "确认上传";
        document.getElementById("_submit").disabled = true;
        document.getElementById("msg").innerHTML = "升级完成，3秒后自动转入登陆页面";
        jtProBar.display(false);
        setTimeout("toLogin()", 4000);
    }
    this.fillDIV.innerHTML = (100 * pct).toFixed(0) + "%";
    this.fillDIV.style.width = fillPixels + "px";
}

syj.ProgressBar.prototype.display = function (v) {
    this.parent.style.display = v == true ? 'block' : 'none';
}

//初始化进度条
function initBar() {
    window.jtProBar = new syj.ProgressBar(document.getElementById("progressBar"), barWidth, "bgBar");
    jtProBar.display(true);
    startAuto();
}

function startAuto() {
    if (window.thread == null)
        window.thread = window.setInterval("updatePercent()", barTimer);
}

function stopAuto() {
    window.clearInterval(window.thread);
    window.thread = null;
}

function updatePercent() {
    if (window.count == null)
        window.count = 0;
    window.count = count % barWidth;
    jtProBar.setPercent(window.count / barWidth);
    window.count++;
}

function toLogin() {
    $.cookie('lstatus', false, {path: '/'});
    document.location = 'http://' + document.domain + "/index.html?tt=" + new Date().getTime();
}

function getLanDHCP() {
    $.ajax({
        type: "POST",
        url: actionUrl + "fname=net&opt=dhcpd&function=get",
        dataType: "JSON",
        success: function (data) {
            if (data.error == 0) {
                $("#lan_ip").val(data.ip).siblings("label").hide();
                $("#lan_mask").val(data.mask).siblings("label").hide();
                $("#lan_ip_start").val(data.start).siblings("label").hide();
                $("#lan_ip_end").val(data.end).siblings("label").hide();
                $("#lanIpMark").val(data.ip);
                if (data.enable == 0) {
                    $("#dhcp_onoff").removeClass('open3');
                }
            } else {
                locationUrl(data.error);
            }
        }
    });
}

function setLanDHCP(ip, mask, start, end, enable, ipMark) {
    $.ajax({
        type: "POST",
        url: actionUrl + "fname=net&opt=dhcpd&function=set&start=" + start + "&end=" + end + "&mask=" + mask + "&ip=" + ip + "&enable=" + enable,
        dataType: "JSON",
        success: function (data) {
            if (data.error != 0) {
                locationUrl(data.error);
                $("#lan_btn").attr('disabled', false);
                $("#lan_btn").val('重新配置');
            } else {
                if (ip == ipMark) {
                    getMsg("配置成功！");
                    $("#lan_btn").attr('disabled', false);
                }
            }
        }
    });
}

//设置路由登录账户
function setUserAccount(pwd) {
    $.ajax({
        type: "POST",
        url: actionUrl + "fname=system&opt=login_account&function=set&user=admin" + "&password=" + pwd,
        dataType: "JSON",
        success: function (data) {
            if (data.error == 0) {
                getMsg("修改成功,请用新密码重新登录！");
                setTimeout(function () {
                    $.cookie('lstatus', false, {path: '/'});
                    document.location = 'http://' + document.domain + '/index.html?tt=' + new Date().getTime();
                }, 3000);
            } else {
                locationUrl(data.error);
            }
        }
    });
}

//获取/授权访客列表
function wifiGuestList(fun, mac, action) {
    var data = "fname=net&opt=vap_host&function=" + fun;
    if (mac != '' && fun == 'set') {
        data = "fname=net&opt=vap_host&function=" + fun + "&mac=" + mac + "&action=" + action;
    }
    $.ajax({
        type: "POST",
        url: actionUrl + data,
        dataType: "JSON",
        success: function(json) {
            if (json.error == 0) {
                if (mac != '' && fun == 'set') {
                    getMsg("设置成功！");
                } else {
                    var len = json.guests.length;
                    var guestList = json.guests;
                    var html = '';
                    var switchClass = '';
                    var guestName = '';
                    if (len > 0) {
                        for (var i = 0; i < len; i++) {
                            if (MODEL[guestList[i].vender] !== undefined) {
                                guestName = MODEL[guestList[i].vender] + "-" + guestList[i].name;
                            } else {
                                guestName = guestList[i].name;
                            }
                            switchClass = switch_open(guestList[i].wlist, 4);
                            html += '<tr><td><div class="name">' + guestName + '</div></td><td><div class="tbDiv"><i class="' + switchClass + '" data=' + guestList[i].mac + '></i></div></td></tr>';
                        }
                        $("#visit_mode_div").show();
                        $("#guestList").empty().html(html);
                    } else {
                        $("#visit_mode_div").hide();
                    }
                }
            } else {
                locationUrl(json.error);
            }
        }
    });
}

function GetProgressBar(text) {
    layer.open({
        type: 1,
        closeBtn: false,
        content: '<div style="padding:10px 20px;background:#fff;text-align:center;">' + text + '</div><div id="progressBar2" class="progress"><div id="progressBar_Track2"></div></div><p id="progressBarTxt2"></p>',
        area: ['260px', '180px']
    });
    var ProgressBar = {
        maxValue: 100,
        value: 0,
        SetValue: function(aValue) {
            this.value = aValue;
            if (this.value >= this.maxValue)
                this.value = this.maxValue;
            if (this.value <= 0)
                this.value = 0;
            var mWidth = this.value / this.maxValue * $("#progressBar2").width() + "px";
            $("#progressBar_Track2").css("width", mWidth);
            $("#progressBarTxt2").html(this.value + "/" + this.maxValue);
        }
    }
    //设置最大值
    ProgressBar.maxValue = 100;
    //设置当前刻度
    var index = 0;
    var mProgressTimer = setInterval(function() {
        index += 0.83;
        if (index > 99) {
            window.clearInterval(mProgressTimer);
            ProgressBar.SetValue(0);
            layer.closeAll();
        }
        ProgressBar.SetValue(index.toFixed(2));
    }, 1000);
    setTimeout(function() {
        $.cookie('lstatus', false, {path: '/'});
        document.location = 'http://' + document.domain + '/index.html?tt=' + new Date().getTime();
    }, 120000);
}

function getHostRaMac(type, Id) {
    if (Id == 'macChoose1') {
        Id = 1;
    } else if (Id == 'macChoose2') {
        Id = 2
    } else if (Id == 'macChoose3') {
        Id = 3;
    }
    if (wanConfigJsonObject.mode != '' || wanConfigJsonObject.mode !== undefined) {
        if (type == 2) {
            $("#mac_box" + Id).show().text(wanConfigJsonObject.hostmac);
        } else if (type == 3) {
            $("#mac_box" + Id).show().text(wanConfigJsonObject.rawmac);
        }
    }
}

function getCloneMac(clone) {
    var mac;
    if (clone == 2) {
        mac = wanConfigJsonObject.hostmac;
    } else if (clone == 3) {
        mac = wanConfigJsonObject.rawmac;
    }
    return mac;
}

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
var lanS = '';
var ticketinter = 0;
var vpnUrl = 'http://vpn.hbdata.cc:8090/vpn/service/data?math=' + Math.random();
var vpnJson;
var vpnJsonObject;
var vpnOnlineStatus;
var intervaldhcp;
var intervalstatus;
var vorder;
var wispError;
var mob = getMobile();
var jsonWifiListObject;
var WanDetectLink;

/*
 * 设置PPPOE账号密码
 */
function setPPPOE(account, passwd, server, mac, mtu, dns, dns1, clone) {
    $("#btn_1").attr("disabled", true);
    $("#btn_1").val("拨号中...");
    if (typeof (wanConfigJsonObject) == 'undefined') {
        getWanConfigJsonObject();
    }
    if (clone > 1) {
        mac = getCloneMac(clone);
        mac = mac.toUpperCase();
    }
    dns = dns.length > 0 ? dns : "";
    dns1 = dns1.length > 0 ? dns1 : "";

    if (wanInfoJsonObject.connected == 0 || currentMode != 2 || mac != myMac || account != wanConfigJsonObject.user || passwd != wanConfigJsonObject.passwd || dns != wanConfigJsonObject.dns || dns1 != wanConfigJsonObject.dns1 || server != wanConfigJsonObject.pppoe_server) {
        var data = "fname=net&opt=wan_conf&function=set&user=" + account + "&passwd=" + passwd + "&mode=2&mtu=" + mtu + "&dns=" + dns + "&dns1=" + dns1 + "&mac=" + mac;
        if (server != '') {
            data = data + '&pppoe_server=' + server;
        }
        $.ajax({
            type: "POST",
            url: actionUrl + data + "&math=" + Math.random(),
            dataType: "json",
            success: function (data) {
                var jsonObject = data;
                if (jsonObject.error == 0) {
                    offlineSet(1);
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
        mac = mac.toUpperCase();
    }

    dns = dns.length > 0 ? dns : "";
    dns1 = dns1.length > 0 ? dns1 : "";

    if (currentMode != 1 || mac != myMac || clone > 0 || dns != wanConfigJsonObject.dns || dns1 != wanConfigJsonObject.dns1 || mtu != wanConfigJsonObject.mtu || wanInfoJsonObject.connected == 0) {
        var data = "fname=net&opt=wan_conf&function=set&mode=1&mtu=" + mtu + "&dns=" + dns + "&dns1=" + dns1 + "&mac=" + mac;
        $.ajax({
            type: "POST",
            url: actionUrl + data + "&math=" + Math.random(),
            dataType: "json",
            success: function (data) {
                var jsonObject = data;
                if (jsonObject.error == 0) {
                    offlineSet(3);
                    currentMode = 1;
                    intervalTime(3, '启用', 'DHCP配置', 1);
                } else {
                    locationUrl(jsonObject.error);
                    $("#btn_3").attr("disabled", false);
                    $("#btn_3").val("重新启用");
                    return;
                }
            }, error: function () {
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
        mac = mac.toUpperCase();
    }

    dns = dns.length > 0 ? dns : "";
    dns1 = dns1.length > 0 ? dns1 : "";

    if (currentMode != 3 || mac != myMac || ip != myStaticIp || mask != myStaticMask || gw != myStaticGw || clone > 0 || dns != wanConfigJsonObject.dns || dns1 != wanConfigJsonObject.dns1 || mtu != wanConfigJsonObject.mtu || wanInfoJsonObject.connected == 0) {
        var data = "fname=net&opt=wan_conf&function=set&ip=" + ip + "&mask=" + mask + "&gw=" + gw + "&mode=3&mtu=" + mtu + "&dns=" + dns + "&dns1=" + dns1 + "&mac=" + mac;
        $.ajax({
            type: "POST",
            url: actionUrl + data + "&math=" + Math.random(),
            dataType: "json",
            success: function (data) {
                var jsonObject = data;
                if (jsonObject.error == 0) {
                    offlineSet(2);
                    currentMode = 3;
                    intervalTime(2, '确认', '静态IP设置', 3);
                } else {
                    locationUrl(jsonObject.error);
                    $("#btn_2").attr("disabled", false);
                    $("#btn_2").val("重新确认");
                    return;
                }
            }, error: function () {
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
 * 3G模式
 */
function setThreeG() {
    $("#btn_5").attr("disabled", true);
    $("#btn_5").val("拨号中...");
    if (typeof (wanConfigJsonObject) == 'undefined') {
        getWanConfigJsonObject();
    }

    if (currentMode != 5 || wanInfoJsonObject.connected == 0) {
        var data = "fname=net&opt=wan_conf&function=set&mac=" + myMac + "&mode=5";
        $.ajax({
            type: "POST",
            url: actionUrl,
            data: data + "&math=" + Math.random(),
            dataType: "json",
            success: function (data) {
                var jsonObject = data;
                if (jsonObject.error == 0) {
                    currentMode = 5;
                    intervalTime(5, '拨号', '3G上网配置', 5);
                } else {
                    locationUrl(data.error);
                    $("#btn_5").attr("disabled", false);
                    $("#btn_54").val("重新拨号");
                    return;
                }
            }, error: function () {
                getMsg('3G上网配置超时,请重新连接路由器！');
                return;
            }
        });
    } else {
        if (wanInfoJsonObject.connected == 0) {
            intervalTime(5, '拨号', '3G上网配置', 5);
        } else {
            toUrl();
        }
    }
}


/*
 * 无线中继
 */
function getWifiList(refresh) {
    if (jsonWifiListObject !== undefined && refresh != 1) {
        if (jsonWifiListObject.aplist.length > 0) {
            wifiHtmlList(jsonWifiListObject);
            return;
        }
    }

    loading(1, '正在加载...');
    if (typeof (wanInfoJsonObject) == 'undefined') {
        getWanInfoJsonObject();
    }

    if (wanInfoJsonObject.connected == 1 && wanInfoJsonObject.mode == 4) {
        $("#wf_name").text(wanConfigJsonObject.ssid);
        $("#noinpt").text("中继成功，网络已连接");
    } else {
        $("#wf_name").text('请选择');
    }
    $.ajax({
        type: 'POST',
        url: actionUrl + "fname=net&opt=ap_list&function=get" + "&math=" + Math.random(),
        dataType: "JSON",
        async: true,
        success: function (data) {
            jsonWifiListObject = data;
            if (jsonWifiListObject.error == 0) {
                wifiHtmlList(jsonWifiListObject);
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
/*
 * 无线中继连接
 * @param {type} ssid
 * @param {type} pwd
 * @param {type} mac
 * @param {type} channel
 */
function connectWisp(ssid, pwd, mac, channel, sec) {
    loading(1, '正在连接...');
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
                    layer.closeAll();
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
        async: false,
        dataType: "json",
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
                    if (jsonObject.reason == 21) {
                        wispError = 21;
                    } else if (jsonObject.reason == 22) {
                        wispError = 22;
                    }
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
                checkWanDetect(wanInfoJsonObject.mode);
            } else if (jsonObject.wan_link == 0) {
                getWanInfoJsonObject();
                if (wanInfoJsonObject.connected == 1) {
                    toUrl();
                } else {
                    layer.closeAll();
                    clearInterval(checkWanDetectLinkInterval);
                    var lineDivShow = $("#lineOff");
                    if (mob == 1) {
                        layer.open({
                            type: 1,
                            shadeClose: false,
                            content: '<div class="wifiConnectLayer" id="lineOff" style="display:block;"><div class="con con2"><div class="img1"><img src="../images/img-1.png"></div><p>检测到WAN口未插网线</p><div class="img2"><img id="go_on" src="../images/img-2.png"></div><div class="box lineOffSet"><a id="lineOffSet" href="javascript:;" class="btn layer-btn">离线配置</a> <a id="lineOffConnect" href="javascript:;" class="btn layer-btn">无线中继</a></div></div></div>',
                            style: 'width:90%;background-color: transparent;  box-shadow: none;'
                        });
                    } else {
                        loading(1, lineDivShow, 1);
                    }
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
        success: function (data) {
            var jsonObject = data;
            if (jsonObject.error == 0) {
                WanDetectLink = jsonObject.wan_link;
                if (jsonObject.wan_link == 0 && type < 1) {
                    var i = 0;
                    checkWanDetectLinkInterval = setInterval(function () {
                        checkWanDetectLink();
                        i++;
                    }, 2000);
                    if (i == 15) {
                        clearInterval(checkWanDetectLinkInterval);
                        layer.closeAll();
                    }
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


function userLogin(str) {
    var name = 'admin';
    var str_md5 = $.md5(name + str);
    $.ajax({
        type: "POST",
        timeout: 3000,
        url: actionUrl + "fname=system&opt=login&function=set&usrid=" + str_md5,
        dataType: "JSON",
        success: function (data) {
            if (data.error == 0) {
                $.cookie('lstatus', true);
                if (mob == 1) {
                    $("#account_login").hide();
                }
                checkConfig();
            } else if (data.error == 10001) {
                getMsg('密码错误！');
                $("#user_login").text('重新登录');
                $("#user_login").attr("disabled", false);
            } else {
                getMsg('登录失败');
                $("#user_login").text('重新登录');
                $("#user_login").attr("disabled", false);
            }
        }, complete: function (XHR, TS) {
            XHR = null;
        },
        error: function (XHRequest, status, data) {
            getMsg(XHRequest.status);
            XHRequest.abort();
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
        success: function (data) {
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
                        if (mob == 1) {
                            $("#routerLed").addClass('close-btn');
                        } else {
                            if ($("#led_on").hasClass('selected') == true) {
                                $("#led_on").removeClass('selected');
                            }
                            if ($("#led_off").hasClass('selected') == false) {
                                $("#led_off").addClass('selected');
                            }
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
        url: actionUrl + "fname=&opt=wifi_lt" + type + "&function=get&math=" + Math.random(),
        dataType: "JSON",
        success: function (data) {
            if (data.error == 0) {
                router.getWifiAp();
                if (mob == 1) {
                    data.sh = data.sh < 10 ? '0' + data.sh : data.sh;
                    data.sm = data.sm < 10 ? '0' + data.sm : data.sm;
                    data.eh = data.eh < 10 ? '0' + data.eh : data.eh;
                    data.em = data.em < 10 ? '0' + data.em : data.em;
                    var startTime = data.sh + ':' + data.sm;
                    var endTime = data.eh + ':' + data.em;
                    $("#start_time").val(startTime);
                    $("#end_time").val(endTime);
                    for (var i = 0; i < data.week.length; i++) {
                        $(".day-choose a").eq(data.week[i]).addClass('selected');
                    }
                } else {
                    $("#startHour option[value=" + data.sh + "]").attr('selected', 'selected');
                    $("#startMin option[value=" + data.sm + "]").attr('selected', 'selected');
                    $("#endHour option[value=" + data.eh + "]").attr('selected', 'selected');
                    $("#endMin option[value=" + data.em + "]").attr('selected', 'selected');
                    var j = 0;
                    $("input[name=day]").each(function (index) {
                        for (var i = 0; i <= data.week.length; i++) {
                            if (index == data.week[i]) {
                                j++;
                                $(this).attr("checked", true);
                            }
                        }
                    });
                    if (j == 7) {
                        $("#day0").attr("checked", "checked");
                    }
                }

                if (data.time_on == 1) {
                    $("#wifiTimeOn").addClass('selected');
                    $("#wifiTimeOff").removeClass('selected');
                    $("#wf_time_div").show();
                } else if (data.time_on == 0) {
                    if (mob == 1) {
                        $("#wifiTimeOnOff").addClass('close-btn');
                    }
                    $("#wifiTimeOff").addClass('selected');
                    $("#wifiTimeOn").removeClass('selected');
                    $("#wf_time_div").hide();
                }

                if (data.enable == 1) {
                    $("#wifi_on").addClass('selected');
                    $("#wifi_off").removeClass('selected');
                    $("#swichDiv").show();
                } else {
                    if (mob == 1) {
                        $("#wifiOnOff").addClass('close-btn');
                        $("#wifiOnOff").parent().next().hide();
                    } else {
                        $("#wifi_off").addClass('selected');
                        $("#wifi_on").removeClass('selected');
                        $("#swichDiv").hide();
                    }
                }
            } else {
                locationUrl(data.error);
            }
        }
    });
}


function wifiSet(enable, time_on, week, sh, sm, eh, em) {
    $.ajax({
        type: "POST",
        url: actionUrl + "fname=net&opt=wifi_lt&function=set&enable=" + enable + "&time_on=" + time_on + "&week=" + week + "&sh=" + sh + "&sm=" + sm + "&eh=" + eh + "&em=" + em + "&math=" + Math.random(),
        dataType: "JSON",
        success: function (data) {
            if (data.error == 0) {
                if (enable == 0) {
                    $("#swichDiv").hide();
                }
                getMsg('设置成功！');
            } else {
                locationUrl(data.error);
            }
        }
    });
}

function wifiTxrateGet(act, rate) {
    var dat = "fname=net&opt=txrate&function=" + act, Class = 'selected';
    if (rate > 0) {
        dat = "fname=net&opt=txrate&function=" + act + "&txrate=" + rate;
    }
    $.ajax({
        type: "POST",
        url: actionUrl + dat + "&math=" + Math.random(),
        dataType: "JSON",
        success: function (data) {
            if (data.error == 0) {
                if (rate > 0) {
                    getMsg('设置成功！');
                }
                var rate_1 = $("#rate_mode_1"), rate_2 = $("#rate_mode_2"), rate_3 = $("#rate_mode_3");
                if (mob == 1) {
                    rate_1 = $("#rate_mode_1").find('span').eq(1);
                    rate_2 = $("#rate_mode_2").find('span').eq(1);
                    rate_3 = $("#rate_mode_3").find('span').eq(1);
                    Class = 'checked';
                }
                if (data.txrate <= 30 || (rate <= 30 && rate != 0)) {
                    rate_1.addClass(Class);
                    rate_2.removeClass(Class);
                    rate_3.removeClass(Class);
                } else if ((data.txrate <= 65 && data.txrate > 30) || (rate <= 65 && rate > 30 && rate != 0)) {
                    rate_2.addClass(Class);
                    rate_1.removeClass(Class);
                    rate_3.removeClass(Class);
                } else if ((data.txrate <= 100 && data.txrate > 65) || (rate <= 100 && rate > 65 && rate != 0)) {
                    rate_3.addClass(Class);
                    rate_1.removeClass(Class);
                    rate_2.removeClass(Class);
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
function wifiGuestGet(act, status, vssid, pwd, hidden, sec) {
    var dat = "fname=net&opt=wifi_vap&function=get";
    if (act == 'set') {
        dat = "fname=net&opt=wifi_vap&function=set" + "&enable=" + status;
        if (vssid != '' && vssid !== undefined) {
            dat = dat + '&vssid=' + vssid;
        }
        if (pwd != '' && pwd !== undefined) {
            dat = dat + '&password=' + pwd;
        }
        if (hidden != '' && hidden !== undefined) {
            dat = dat + '&hidden=' + hidden;
        }
    }
    $.ajax({
        type: "POST",
        url: actionUrl + dat,
        dataType: "JSON",
        success: function (data) {
            if (data.error == 0) {
                if (act == 'get' && data.enable == 1) {
                    $("#guest_on").addClass('selected');
                    $("#guest_off").removeClass('selected');
                    $("#wf_vnameset").val(data.vssid).siblings("label").hide();
                    if (trim(data.password) != '' && data.security == 1) {
                        $('#wf_vpwdset').val(data.password).siblings("label").hide();
                    } else if (data.security == 0) {
                        $('#wf_vpwdset').val('').siblings("label").show();
                    }
                    if (data.hidden == 1) {
                        $("#hidden_vssid").attr("checked", true);
                    }
                    wifiGuestList('get');
                } else if (act == 'get' && data.enable == 0) {
                    if (mob == 1) {
                        $(".swichBtn").addClass('close-btn');
                        $("#visit_mode_div").hide();
                        $("#guestList").hide();
                    } else {
                        $("#guest_off").addClass('selected');
                        $("#guest_on").removeClass('selected');
                        $("#visit_mode_div").hide();
                    }
                    $("#reGuestList").hide();
                    $("#vset").hide();
                } else {
                    getMsg('设置成功！');
                    if (act == 'set' && status == 1) {
                        wifiGuestGet('get');
                        wifiGuestList('get');
                        if (mob == 1) {
                            $("#visit_mode_div").show();
                            $("#guestList").show();
                        } else {
                            $("#reGuestList").show();
                        }
                        $("#vset").show();
                    } else if (act == 'set' && status == 0) {
                        if (mob == 1) {
                            $("#visit_mode_div").hide();
                            $("#guestList").hide();
                        } else {
                            $("#reGuestList").hide();
                        }
                        $("#vset").hide();
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
        success: function (data) {
            if (data.error == 0) {
                var html = '';
                if (fc == 'get') {
                    for (var i = 0; i < data.nat.length; i++) {
                        data.nat[i].mark = switch_open(data.nat[i].enable, 2);
                        data.nat[i].protoMark = changeTcpUdp(data.nat[i].proto);
                        if (mob == 1) {
                            html += "<ul class='port-list bgfff'><li>" + data.nat[i].out_port + "</li><li>" + data.nat[i].in_port + "</li><li>" + data.nat[i].protoMark + "</li><li><a href='javascript:;' class='" + data.nat[i].mark + "'></a></li><li class='editBtn'><a class='remove' href='javascript:;' style='display:block;'>删除</a></li></ul>";
                        } else {
                            html += "<tr><td>" + data.nat[i].out_port + "</td><td>" + data.nat[i].in_port + "</td><td>" + data.nat[i].protoMark + "</td><td class='mod_nat'><div class='tbDiv'><i class='" + data.nat[i].mark + "'></i></div></td><td class='del_nat'><div class='editBtn remove'></div></td></tr>";
                        }
                    }
                    $("#host_nat").empty().html(html);
                } else if (fc == 'set') {
                    if (act == 'del') {
                        getMsg('删除成功！');
                        if (mob == 1) {
                            $("#host_nat").find('ul').eq(tr).remove();
                        } else {
                            $("#host_nat").find('tr').eq(tr).remove();
                        }
                    } else if (act == 'add') {
                        getMsg('添加成功！');
                        var mark = switch_open(enable, 2);
                        proto = changeTcpUdp(proto);
                        $("#dk_out").val('');
                        $("#dk_inner").val('');
                        if (mob == 1) {
                            html += "<ul class='port-list bgfff'><li>" + out_port + "</li><li>" + in_port + "</li><li>" + proto + "</li><li><a href='javascript:;' class='" + mark + "'></a></li><li class='editBtn'><a class='remove' href='javascript:;' style='display:block;'>删除</a></li></ul>";
                        } else {
                            html = "<tr><td>" + out_port + "</td><td>" + in_port + "</td><td>" + proto + "</td><td class='mod_nat'><div class='tbDiv'><i class='" + mark + "'></i></div></td><td class='del_nat'><div class='editBtn remove'></div></td></tr>";
                        }
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
        success: function (data) {
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
                checkWanDetectLink();
            } else {  //已配置
                isConfiged = true;
                if (wanInfoJsonObject.link == 0) {
                    var i = 0;
                    checkWanDetectLinkInterval = setInterval(function () {
                        checkWanDetectLink();
                        i++;
                    }, 2000);
                    if (i == 15) {
                        clearInterval(checkWanDetectLinkInterval);
                        layer.closeAll();
                    }
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
        $("#login_pwd").css("visibility", "hidden");
        $("#login_pwd_label").click(function () {
            $(this).siblings(".inpt").css("visibility", "visible");
        }).focusout(function () {
            $(this).siblings(".inpt").css("visibility", "hidden");
        });
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
        url: actionUrl,
        data: "fname=net&opt=wan_account&function=set",
        dataType: "json",
        success: function (data) {
            var jsonObject = data;
            if (jsonObject.error == 0) {
                $.ajax({
                    type: "POST",
                    url: actionUrl,
                    data: "fname=net&opt=wan_account&function=get",
                    dataType: "json",
                    success: function (data) {
                        var jsonObject = data;
                        if (jsonObject.error == 0) {
                            var mob = getMobile();
                            layer.closeAll();
                            if (mob == 1) {
                                layer.open({
                                    type: 1,
                                    title: false,
                                    shade: [0.7, '#000'],
                                    closeBtn: false,
                                    content: "<div class='wifiConnectLayer' style='height:auto;display:block;' id='sucLayer'><div class='bg'></div><div class='con con2'><div class='layer-suc png_ie'></div><p>获取成功</p><p>账号：<em id='account_get'></em></p><p>密码：<em id='pwd_get'></em></p><div class='box'><a id='net_work' class='btn layer-btn'>去拨号</a></div></div></div>",
                                    style: 'width:90%;background-color: transparent;  box-shadow: none;'
                                });
                            } else {
                                layer.open({
                                    type: 1,
                                    title: false,
                                    shade: [0.7, '#000'],
                                    closeBtn: false,
                                    content: $('#sucLayer'),
                                    skin: 'cy-class'
                                });
                            }
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
                                        }, 1200)
                                    }
                                    getAccountTimer();
                                    timeout++;
                                }
                            }, 3000);
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
        url: actionUrl,
        data: "fname=net&opt=wan_account&function=get",
        dataType: "json",
        success: function (data) {
            var jsonObject = data;
            if (jsonObject.error == 0) {
                var mob = getMobile();
                clearInterval(interval);
                isGet = true;
                timeout = 0;
                layer.closeAll();
                if (mob == 1) {
                    layer.open({
                        type: 1,
                        title: false,
                        shade: [0.7, '#000'],
                        closeBtn: false,
                        content: "<div class='wifiConnectLayer' style='height:auto;display:block;' id='sucLayer'><div class='bg'></div><div class='con con2'><div class='layer-suc png_ie'></div><p>获取成功</p><p>账号：<em id='account_get'></em></p><p>密码：<em id='pwd_get'></em></p><div class='box'><a id='net_work' class='btn layer-btn'>去拨号</a></div></div></div>",
                        style: 'width:90%;background-color: transparent;  box-shadow: none;'
                    });

                } else {
                    layer.open({
                        type: 1,
                        title: false,
                        shade: [0.7, '#000'],
                        closeBtn: false,
                        content: $('#sucLayer'),
                        skin: 'cy-class'
                    });
                }
                $('#account_get').empty().text(jsonObject.account);
                $('#pwd_get').empty().text(jsonObject.passwd);
            }
        }
    });
}

function intervalTime(btn, btnText, msgText, mode) {
    if (WanDetectLink == 0 && mode != 4 && mode != 5) {
        getMsg('离线配置成功！');
        $("#btn_" + btn).attr("disabled", false);
        $("#btn_" + btn).val("重新" + btnText);
        return false;
    }
    wispError = 0;
    responseReason = 0;
    var timeoutCount = 59;
    if (mode == 4) {
        timeoutCount = 29;
    }
    var buttonId = typeButton(mode), second = 0;
    interval = setInterval(function () {
        second++;
        getNetMessage(second * 4, buttonId, msgText);
        if (responseReason > 0 && responseReason != 20) {
            $("#" + buttonId + '_message').text(getErrorCode(responseReason) + ',' + second * 4 + 's').css('color', "#dc4b4b");
        }
        getWanInfoJsonObject();
        if ((wanInfoJsonObject.link == 0 && mode != 4 && mode != 5) || responseReason == 19) {
            timeout = 59;
        }
        if (timeout > timeoutCount - 1) {
            if (wanInfoJsonObject.error == 0) {
                if (wanInfoJsonObject.connected == 0) {
                    if (wanInfoJsonObject.link == 0 && mode != 4 && mode != 5) {
                        getMsg("请插入网线！");
                        $("#" + buttonId + '_message').text('请插入网线！').css('color', "#dc4b4b");
                    } else {
                        if (responseReason > 0) {
                            getMsg(errorMessage(responseReason));
                            $("#" + buttonId + '_message').text(errorMessage(responseReason)).css('color', "#dc4b4b");
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
            if (mode == 4 && wanInfoJsonObject.connected == 0) {
                layer.closeAll();
                getMsg('无线中继' + errorMessage(wispError));
                $("#noinpt").text('中继失败：' + errorMessage(wispError));
            }
            $("#btn_" + btn).attr("disabled", false);
            $("#btn_" + btn).val("重新" + btnText);
            timeout = 0;
            clearInterval(interval);
            return;
        }
        if (wanInfoJsonObject.connected == 0 || wanInfoJsonObject.mode != mode) {
            timeout++;
            if (mode == 4 && (wispError == 21 || wispError == 22)) {
                $("#content_message").text(getErrorCode(wispError));
            }
        } else if (wanInfoJsonObject.connected == 1 && wanInfoJsonObject.mode == mode) {
            toUrl();
        }
    }, 4000);
}

function checkVersion() {
    $.ajax({
        type: "POST",
        url: actionUrl + "fname=system&opt=firmstatus&function=get",
        dataType: "JSON",
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
        }
    });
}

function getGradeStatus() {
    $("#update_1").val('正在下载');
    $("#update_1").attr('disabled', true);
    $.ajax({
        type: "POST",
        url: actionUrl + "fname=system&opt=firmstatus&function=set&flag=get&math=" + Math.random(),
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
                var content = '正在升级，可能需要两分钟。升级过程中请勿操作或断电,升级完成后请重新登录!';
                GetProgressBar(content);
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
                document.getElementById("msg").innerHTML = "正在升级，大约需要3分钟。升级过程中请勿操作或断电...";
                initBar();
            } else {
                document.getElementById("_submit").value = "重新上传";
                interval = 0;
                isGet = false;
                document.getElementById("_submit").disabled = false;
                document.getElementById("msg").innerHTML = "升级遇到错误，请联系客服！错误码：" + jsonObject.error;
            }
        }
    });
}


function uploadFile() {
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
        url: actionUrl + "fname=system&opt=up_ready&function=set&math=" + Math.random(),
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
        url: actionUrl + "fname=net&opt=upload_status&function=get&math=" + Math.random(),
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
                if (trim(data.dns1) != '') {
                    $("#lan_dns1").val(data.dns1).siblings("label").hide();
                }
                if (trim(data.dns2) != '') {
                    $("#lan_dns2").val(data.dns2).siblings("label").hide();
                }
                $("#lanIpMark").val(data.ip);
                if (data.enable == 0) {
                    if (mob == 1) {
                        $("#dhcp_onoff").addClass('close-btn');
                    } else {
                        $("#dhcp_onoff").removeClass('open3');
                    }
                }
            } else {
                locationUrl(data.error);
            }
        }
    });
}

function setLanDHCP(ip, mask, start, end, enable, ipMark, dns1, dns2) {
    $.ajax({
        type: "POST",
        url: actionUrl + "fname=net&opt=dhcpd&function=set&start=" + start + "&end=" + end + "&mask=" + mask + "&ip=" + ip + "&enable=" + enable + "&dns1=" + dns1 + "&dns2=" + dns2,
        dataType: "JSON",
        timeout: 10000,
        success: function (data) {
            layer.closeAll();
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
        }, error: function (XHRequest, status, data) {
            layer.closeAll();
            getMsg("已配置！");
            $("#lan_btn").attr('disabled', false);
            XHRequest.abort();
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
                    $.cookie('lstatus', false, {path: '/m'});
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
        success: function (json) {
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
                            if (mob == 1) {
                                html += '<li><div class="v-name">' + guestName + '</div><div class="v-ope"><a href="javascript:;" class="' + switchClass + '" data=' + guestList[i].mac + '></a></div></li>';
                            } else {
                                html += '<tr><td><div class="name">' + guestName + '</div></td><td><div class="tbDiv"><i class="' + switchClass + '" data=' + guestList[i].mac + '></i></div></td></tr>';
                            }
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
        SetValue: function (aValue) {
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
    var mProgressTimer = setInterval(function () {
        index += 0.83;
        if (index > 99) {
            window.clearInterval(mProgressTimer);
            ProgressBar.SetValue(0);
            layer.closeAll();
        }
        ProgressBar.SetValue(index.toFixed(2));
    }, 1000);
    setTimeout(function () {
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
    if (mac == '' || mac === undefined) {
        mac = wanConfigJsonObject.rawmac;
    }
    return mac;
}

function getTicket(type) {
    var dat = "fname=sys&opt=lanxun&function=get";
    if (type >= 0 && type !== '' && type !== undefined) {
        dat = "fname=sys&opt=lanxun&function=set&enabled=" + type;
    }
    $.ajax({
        type: "POST",
        url: actionUrl + dat,
        dataType: "JSON",
        success: function (data) {
            if (data.error == 0) {
                lanS = data.status;
                if (type !== '') {
                    getMsg('设置成功');
                    if (type == 1) {
                        $('.lanStatus').text('正在运行...');
                        $('.qticket').empty().text('关闭');
                        setTimeout(function () {
                            loading(2, '正在加载...');
                        }, 3000);
                        var tinter = 15;
                        ticketinter = setInterval(function () {
                            tinter--;
                            if (tinter > 0) {
                                getTicket('');
                            } else {
                                window.clearInterval(ticketinter);
                                layer.closeAll();
                                getMsg('加载超时,请刷新！');
                            }
                        }, 5000);
                    } else {
                        lanS = 0;
                        $('.lanStatus').text('已关闭...');
                        $('.qticket').empty().text('开启');
                        $("#ldomain").empty();
                        if ($(".lstatus").hasClass('lano') === false) {
                            $(".lstatus").addClass('lano');
                        }
                    }
                } else {
                    if (lanS == 1) {
                        $('.lanStatus').text('正在运行...');
                        $('.qticket').empty().text('关闭');
                    } else {
                        $('.qticket').empty().text('开启');
                        $('.lanStatus').text('已关闭...');
                    }
                    var html = '', liclass, listatus = '', j = 0, k = 0, h = 0;
                    var info = data.info;
                    if (info !== undefined) {
                        $("#probing_times").text(info.probing_times);
                        $.each(info, function (name, value) {
                            h++;
                            if (typeof (value.info) == 'object') {
                                j++;
                                var lihtml = '', lispeed = '';
                                for (var i = 0; i < value.info.length; i++) {
                                    k++;
                                    if (value.info[i].status == 'best') {
                                        liclass = 'green';
                                        listatus = '(最优)';
                                    } else if (value.info[i].status == 'normal') {
                                        liclass = 'grey';
                                        listatus = '(正常)';
                                    } else if (value.info[i].status == 'default') {
                                        liclass = 'blue';
                                        listatus = '(默认)';
                                    }
                                    lihtml += '<li class="' + liclass + '">' + value.info[i].ip + listatus + '</li>';
                                    lispeed += '<li class="' + liclass + '">' + value.info[i].latency + '</li>';
                                }
                                html += '<tr><td valign="top">' + value.domain + '</td><td><ul>' + lihtml + '</ul></td><td><ul>' + lispeed + '</ul></td></tr>';
                            }
                        });
                        if (j > 0) {
                            window.clearInterval(ticketinter);
                            layer.closeAll();
                            $(".lstatus").removeClass('lano');
                            getMsg('加载成功！');
                        }
                        var parcent = (j / k * 100).toFixed(2);
                        $("#parcent").text(parcent + '%');
                        $("#ldomain").empty().html(html);
                    }
                    if (lanS == 0) {
                        if ($(".lstatus").hasClass('lano') === false) {
                            $(".lstatus").addClass('lano');
                        }
                        $("#ldomain").empty();
                    } else if (lanS == 1 && (info === undefined || h == 1) && ticketinter == 0) {
                        getMsg('正在运行,请等待...');
                    }
                }
            } else {
                locationUrl(data.error);
            }
        }, error: function (XHRequest, status, data) {
            getMsg('错误信息:', status, '请重新连接路由器');
            XHRequest.abort();
        }
    });
}

function getVpn(param, type) {
    var par = param;
    param = JSON.stringify(param);
    $.support.cors = true;
    $.ajax({
        type: "POST",
        timeout: 10000,
        url: vpnUrl,
        data: param,
//        async: false,
        dataType: "json",
        success: function (data) {
            vpnJson = data;
            if (type == 1) {
                getMsg(data.des);
                if (vpnJson.code == '110') {
                    vpnOnline(2);
                    vpnInfo('get');
                    if (par.open == '1' && vpnJson.result == 0) {
                        $('.startvpn').val('关闭');
                    } else if (par.open == 0) {
                        $('.startvpn').val('开启');
                    }
                    if (vpnJsonObject.connect == 1 && vpnJson.open == 1) {
                        $('#vpstatus').text('已启用');
                    } else if (par.open == 1 && vpnJsonObject.connect == 0) {
                        $('#vpstatus').text('拨号中');
                        var i = 0;
                        intervalstatus = setInterval(function () {
                            getvpnInfocid(1);
                            i++;
                            if (i == 30) {
                                if (vpnJsonObject.connect == 0) {
                                    getMsg('拨号超时,请重新拨号！');
                                    $('#vpstatus').text('拨号超时');
                                    if (i > 150) {
                                        clearInterval(intervalstatus);
                                    }
                                } else if (vpnJsonObject.connect == 1) {
                                    $('#vpstatus').text('已连接');
                                    clearInterval(intervalstatus);
                                }
                            }
                        }, 4000);
                    }
                    if (par.open == 0) {
                        $('#vpstatus').text('未启用');
                    }
                } else if (vpnJson.code == '103' && vpnJson.result == '0') {
                    vpnInfo('set', par.phone, $("#pass_login").val());
                    VpnStatus();
                } else if (vpnJson.code == '108' && vpnJson.result == '0') {
                    VpnStatus();
                } else if (vpnJson.code == '104' && vpnJson.result == '0') {
                    getMsg('密码重置成功！');
                    setTimeout(function () {
                        layer.closeAll();
                    }, 2000);
                } else if (vpnJson.code == '114' && vpnJson.result == '0') {
                    closeClient();
                }
            } else if (type == 2 && vpnJson.code == '107') {
                if (data.result == 0 && data.method == 2) {
                    var pname = $("#vpnshowlist").find('.selected').text();
                    $("#packageName").text(pname);
                    $("#payMehtod").text('微信扫描二维码支付');
                    $("#payMoney").text($("#orderpice").text());
                    $("#toPay").attr('href', data.url);
                    loading(1, $("#confirm_to_pay"), 2);
                    vorder = vpnJson.order;
                } else if (data.result == 0 && data.method == 4) {
                    getMsg(data.des);
                    $("#activation").val('');
                    setTimeout(function () {
                        VpnStatus();
                    }, 2000);
                } else {
                    getMsg(data.des);
                    setTimeout(function () {
                        if (data.result == '133' || data.result == '135') {
                            $("#packageshow").hide();
                            $("#vpnpackages").hide();
                            $("#vpn_login").show();
                        }
                    }, '3000');
                }
            } else if (type == 2 && vpnJson.code == '102') {
                if (vpnJson.result == 0) {
                    vpnInfo('set', par.phone, $("#vpnpass").val());
                    getMsg('注册成功！');
                    setTimeout(function () {
                        layer.closeAll();
                        VpnStatus();
                    }, 3000);
                } else {
                    getMsg(vpnJson.des);
                }
            } else if (type == 2 && vpnJson.code == '111') {
                vpnOnlineStatus = vpnJson.open;
            }
        }, error: function (XMLHttpRequest, TextStatus) {
            if (TextStatus == 'error') {
                var ie_version = parseInt($.browser.version.substr(0, 1));
                if (ie_version < 9 && $.browser.msie) {
                    getMsg('浏览器版本过低，请升级到IE9以上版本！');
                } else {
                    getMsg('服务器连接超时，请刷新重试！');
                }
                XMLHttpRequest.abort();
            }
        }
    });
}

function VpnStatus() {
    vpnInfo('get');
    var param = {};
    param.code = '109';
    param.product = '0001';
    param.phone = vpnJsonObject.user;
    param.pass = $.md5(vpnJsonObject.password);
    if (param.phone == '') {
//        loading(1, $("#vpn_login"), 2);
        $("#vpn_login").show();
        return;
    } else {
        $("#vpn_login").hide();
        $(".vaccount").text(vpnJsonObject.user);
        if ($("#packageshow").is(":visible")) {
            $("#packageshow").hide();
        }
        layer.closeAll();
    }
    getVpn(param, 0);
    if (vpnJson === undefined) {
        return;
    }
    var list = vpnJson.packages;
    var listCode = vpnJson.code;
    if (list === undefined) {
        VpnPackage();
    } else if (list.length > 0) {
        vpnOnline(2);
        vpnhtml(list, listCode);
        if (vpnJsonObject.connect == 1 && vpnJson.open == 1) {
            $('#vpstatus').text('已启用');
        } else if (vpnJsonObject.connect == 0 && vpnOnlineStatus == 1) {
            $('#vpstatus').text('拨号中');
        } else {
            $('#vpstatus').text('未启用');
        }
    }
}

function VpnPackage() {
    var param = {};
    param.code = '106';
    param.product = '0001';
    getVpn(param, 0);
    if (vpnJson === undefined) {
        return;
    }
    var list = vpnJson.packages;
    if (list.length > 0) {
        vpnhtml(list, vpnJson.code);
        $("#hvpnp").hide();
        $("#packageshow").show();
    }
}

function vpnhtml(list, code) {
    var html = '', ptime = '';
    var startButton = '', vstatus = '';
    var lclass = '', pclass = '', p2class = '', tn = '', stime = '', etime = '';
    var k = 0;
    for (var i = 0; i < list.length; i++) {
        if (code == '106') {
            k++;
            if (k == 2) {
                lclass = 'tc-name selected';
                pclass = 'tc-name-box';
            } else {
                lclass = 'tc-name';
                pclass = 'tc-name-box hide';
            }
            html += '<a href="javascript:;" data-id="tc_' + k + '" class="' + lclass + '">' + list[i].name + '</a>';
            if (list[i].data !== undefined) {
                var p2time = '', j = 0;
                for (var l = 0; l < list[i].data.length; l++) {
                    if (list[i].data[l].unit == 'D') {
                        tn = '天';
                    } else if (list[i].data[l].unit == 'M') {
                        tn = '个月';
                    }
                    j++;
                    if (j == 1) {
                        p2class = 'tc-time selected';
                    } else {
                        p2class = 'tc-time';
                    }
                    p2time += '<a href="javascript:;" data-id="' + list[i].data[l].id + '" data-price="' + list[i].data[l].price + '" data-desc="' + list[i].data[l].content + '" class="' + p2class + '">' + list[i].data[l].time + tn + '</a>';
                }
                ptime += '<div class="' + pclass + '" id="tc_' + k + '_box">' + p2time + '</div>';
            }
        } else if (code == '109' && list[i].resttime >= 0) {
            k++;
            if (k == 1) {
                if (vpnOnlineStatus == 0) {
                    startButton = '开启';
                    vstatus = '未启用';
                } else if (vpnOnlineStatus == '1') {
                    startButton = '关闭';
                    vstatus = '已启用';
                }

                stime = strtimeFormat(list[i].stime);
                etime = strtimeFormat(list[i].etime);
                html += '<div class="tc-box"><p><strong>当前套餐：</strong>' + list[i].name + '</p><p><strong>剩余时间：</strong>' + list[i].resttime + '天</p><p><strong>开始时间：</strong>' + stime + '</p><p><strong>结束时间：</strong>' + etime + '</p><p><strong>套餐详情：</strong>' + list[i].content + '</p><p><strong>连接状态：</strong><em id="vpstatus">' + vstatus + '</em></p><div class="clear"></div><a href="javascript:;" data-id=' + list[i].id + ' class="rpackageshow">续费>></a></div></div><div class="vpn-btn"><input type="button" class="btn startvpn" name="start_vpn" data-mac="' + list[i].mac + '" id=' + list[i].id + ' value=' + startButton + '></div>';
            }
        }
    }
    if (code == '106') {
        $("#vpnshowlist").empty().html(html);
        $(".pacakage_date").empty().append(ptime);
        $("#vpnpackages").show();
        var tid = $("#vpnshowlist").find('.selected').attr('data-id');
        var price = $("#" + tid + "_box").find('.selected').attr('data-price');
        var pdesc = $("#" + tid + "_box").find('.selected').attr('data-desc');
        $("#orderpice").empty().text(price);
        $('.tc-txt').empty().text(pdesc);
        $('#packageshow').show();
    } else if (code == '109') {
        $("#hvpnp").show();
        $("#hvpnlist").empty().html(html).show();
    }
}

function vpnInfo(type, user, pwd) {
    var dat = 'fname=net&opt=vpn_info&function=get';
    if (type == 'set') {
        dat = 'fname=net&opt=vpn_info&function=set&user=' + user + '&password=' + pwd;
    }
    $.ajaxSettings.async = false;
    $.post(actionUrl + dat, '', function (data) {
        if (data.error == 0) {
            if (type == 'get') {
                vpnJsonObject = data;
                if (data.cid == '' || data.cid === undefined) {
                    loading(2, '正在连接星际互联服务器...', 3);
                    var i = 0;
                    intervaldhcp = setInterval(function () {
                        getvpnInfocid(0);
                        i++;
                        if (i == 15) {
                            layer.closeAll();
                            getMsg('星际互联服务器异常！');
                            if (user == '') {
                                $("#vpn_login").show();
                            }
                        } else if (i > 150) {
                            clearInterval(intervaldhcp);
                        }
                    }, 4000);
                }
            } else if (type == 'set' && vpnJsonObject.user != user && vpnJsonObject.user != '' && vpnJsonObject.user !== undefined && user != '') {
                openStatus(1);
            }
        } else {
            locationUrl(data.error);
            return;
        }
    }, 'json');
}

function getvpnInfocid(type) {
    $.ajaxSettings.async = false;
    $.get(actionUrl + 'fname=net&opt=vpn_info&function=get', function (data) {
        if (type == 0) {
            if (data.error == 0 && data.cid != '' && data.cid !== undefined) {
                vpnJsonObject = data;
                layer.closeAll();
                clearInterval(intervaldhcp);
            }
        } else if (type == 1) {
            if (data.connect == 1 && vpnJson.open == 1) {
                $("#vpstatus").text('已启用');
                clearInterval(intervalstatus);
            }
        }
    }, 'json');
}

function vpnOnline(status) {
    var param = {};
    param.code = '111';
    param.product = '0001';
    param.phone = vpnJsonObject.user;
    param.pass = $.md5(vpnJsonObject.password);
    param.cid = vpnJsonObject.cid;
    getVpn(param, status);
}

//function loading(num, content_text, type) {
//    var content;
//    if (type == 1 || type == 2) {
//        content = content_text;
//    } else {
//        content = '<div class="loading"><p><img src="../images/loading-' + num + '.gif" width="120"></p><p>' + content_text + '</p></div>';
//    }
//    if (mob == 1) {
//        layer.open({
//            type: 1,
//            shadeClose: false,
//            content: '<div class="loading"><p><img src="../../images/loading-1.gif"></p><p>正在检测上网方式...</p></div>',
//            style: 'background-color: transparent;  box-shadow: none;'
//        });
//    } else {
//        layer.open({
//            type: 1,
//            title: false,
//            shade: [0.7, '#000'],
//            closeBtn: false,
//            shadeClose: false,
//            content: content,
//            skin: 'cy-class'
//        });
//    }
//}

function openStatus(type) {
    var param = {};
    param.code = '110';
    param.product = '0001';
    param.phone = vpnJsonObject.user;
    param.pass = $.md5(vpnJsonObject.password);
    param.cid = vpnJsonObject.cid;
    param.id = $('.startvpn').attr('id');
    param.open = '1';
    var timestamp = JSON.stringify(Date.parse(new Date()));
    param.timestamp = timestamp.substr(0, 10);
    clearInterval(intervalstatus);
    if (type == 1) {
        if (vpnOnlineStatus == '1') {
            param.open = '0';
            getVpn(param, 2);
        }
    } else if (type == 2) {
        if (vpnOnlineStatus == '1') {
            param.open = '0';
        }
        getVpn(param, 1);
    }
}

function vHistoryList(code) {
    var param = {};
    param.code = code;
    param.product = '0001';
    param.phone = vpnJsonObject.user;
    param.pass = $.md5(vpnJsonObject.password);

    getVpn(param, 1);
}

function closeService() {
    if (vpnJson.code == 106) {
        vpnOnline(2);
    }
    if (vpnJson.open == 1) {
        vpnInfo('get');
        var param = {};
        param.product = '0001';
        param.code = '114';
        param.phone = vpnJsonObject.user;
        param.pass = $.md5(vpnJsonObject.password);
        param.cid = vpnJsonObject.cid;

        getVpn(param, 1);
    } else {
        closeClient();
    }
    clearInterval(intervalstatus);
}

function closeClient() {
    vpnInfo('set', '', '');
    $("#vpn_login").show();
    $("#packageshow").hide();
    $("#hvpnp").hide();
    $("#packageshow").hide();
    $("#vpnpackages").hide();
    $("#phone_login").val('').siblings("label").show();
    $("#pass_login").val('').siblings("label").show();
}

function strtimeFormat(stime) {
    stime = stime.substr(0, 4) + '-' + stime.substr(4, 2) + '-' + stime.substr(6, 2);
    return stime;
}

function system_log(type) {
    var requrl = actionUrl + 'fname=sys&opt=sys_log&function=get';
    if (type == 1) {
        requrl = actionUrl + 'fname=sys&opt=sys_log&function=get&download=1';
    } else {
        loading(2, '正在加载...');
    }
    $.get(requrl, function (json) {
        if (json.error == 0) {
            if (type != 1) {
                if (json.count > 0) {
                    var logs = json.logs;
                    var html = '';
                    var logtime;
                    for (var i = 0; i < json.count; i++) {
                        logtime = getLocalTime(logs[i].time);
                        if (i % 2 == 0) {
                            if (mob == 1) {
                                html += '<li><div>' + logtime + '</div><div>' + logs[i].event + '</div></li>';
                            } else {
                                html += '<tr class="even"><td width="300">' + logtime + '</td><td class="nobd">' + logs[i].event + '</td></tr>';
                            }
                        } else {
                            if (mob == 1) {
                                html += '<li><div>' + logtime + '</div><div>' + logs[i].event + '</div></li>';
                            } else {
                                html += '<tr><td width="300">' + logtime + '</td><td class="nobd">' + logs[i].event + '</td></tr>';
                            }
                        }
                    }
                    $("#count_log").empty().text(json.count);
                    $("#log_list").empty().html(html);
                    layer.closeAll();
                }
            }
        } else {
            locationUrl(json.error);
        }
    }, 'json');
}

function removeDownClient(tr, mac) {
    var dat = 'fname=net&opt=host_del_history&function=set&mac=' + mac;
    $.ajax({
        type: "POST",
        url: actionUrl + dat,
        dataType: "JSON",
        success: function (json) {
            if (json.error == 0) {
                getMsg('离线设备移除成功！');
                $("#devices").find('tr').eq(tr).remove();
            } else {
                locationUrl(json.error);
            }
        }
    });
}

function getSetPrice(set, price) {
    var dat = 'fname=net&opt=advert&function=get';
    if (set == 'set') {
        dat = 'fname=net&opt=advert&function=set&enable=' + price;
    }
    $.ajax({
        type: "POST",
        url: actionUrl + dat,
        async: false,
        dataType: "JSON",
        success: function (data) {
            if (data.error == 0) {
                if (set == 'get') {
                    if (mob == 1) {
                        if (data.enable == 0) {
                            $("#getPriceSet").addClass('close-btn');
                        }
                    } else {
                        if ($("#buy_on").hasClass('selected') == false && data.enable == 1) {
                            $("#buy_on").addClass('selected');
                            $("#buy_off").removeClass('selected');
                        } else if ($("#buy_off").hasClass('selected') == false && data.enable == 0) {
                            $("#buy_off").addClass('selected');
                            $("#buy_on").removeClass('selected');
                        }
                    }
                } else {
                    if (price == 1) {
                        getMsg('开启成功！');
                        if ($("#buy_on").hasClass('selected') == false) {
                            $("#buy_on").addClass('selected');
                        }
                        $("#buy_off").removeClass('selected');
                    } else {
                        getMsg('关闭成功！');
                        if ($("#buy_off").hasClass('selected') == false) {
                            $("#buy_off").addClass('selected');
                        }
                        $("#buy_on").removeClass('selected');
                    }
                }
            } else {
                locationUrl(data.error);
            }
        }
    });
}

function getNetMessage(sec, buttonId, msgText, mode) {
    if (sec >= 60) {
        if (mode == 2) {
            msgText = msgText + '运营商无响应，正在继续尝试...';
        } else {
            msgText = msgText + '比平时时间要长';
        }
    }
    return $("#" + buttonId + '_message').text(msgText + '，' + sec + 's').css('color', "#12a649");
}

function wifiHtmlList(jsonWifiListObject) {
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
            if (trim(aplist[i].ssid).length >= 1) {
                if (mob == 1) {
                    list = " <li ssid='" + aplist[i].ssid + "' channel=" + aplist[i].channel + " sec=" + aplist[i].security + "><a href='javascript:;'><span class='fl name'>" + data_ssid[i] + "</span><span class='fr'>" + aplist[i].security_lock + "</span></a></li>";
//                    list = "<tr style='cursor: pointer' ssid='" + aplist[i].ssid + "' channel=" + aplist[i].channel + " sec=" + aplist[i].security + "><td width='143' title='" + aplist[i].ssid + "'>" + data_ssid[i] + "</td><td width='170'>" + aplist[i].bssid.toUpperCase() + "</td><td width='40'>" + aplist[i].channel + "</td><td width='230'>" + aplist[i].security + "</td><td>" + aplist[i].security_lock + "</td></tr>";
                } else {
                    list = "<tr style='cursor: pointer' ssid='" + aplist[i].ssid + "' channel=" + aplist[i].channel + " sec=" + aplist[i].security + "><td width='143' title='" + aplist[i].ssid + "'>" + data_ssid[i] + "</td><td width='170'>" + aplist[i].bssid.toUpperCase() + "</td><td width='40'>" + aplist[i].channel + "</td><td width='230'>" + aplist[i].security + "</td><td>" + aplist[i].security_lock + "</td></tr>";
                }
                wifi += list;
            }
        }
    }
    layer.closeAll();
    $("#getWifiList").empty().html(wifi);
}


function offlineSet(btn) {
    if (WanDetectLink == 0) {
        getMsg('离线配置成功！');
        $("#btn_" + btn).attr("disabled", false);
        $("#btn_" + btn).val("重新启用");

        return false;
    }
}

function getParameter(type) {
    $.post(actionUrl + 'fname=sys&opt=command_log&function=set&act=' + type, function (data) {
        if (data.error == 0) {
            if (type == 'on') {
                document.location = 'http://' + document.domain + '/command_log';
                setTimeout(function () {
                    getParameter('off');
                }, 2000);
            }
        } else {
            locationUrl(data.error);
        }
    }, 'json');
}
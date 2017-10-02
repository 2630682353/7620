var ap_router = function () {
    var _this = this;
    this.baseUrl = document.domain + '/protocol.csp?';
    this.wanInfoJsonObject;
    this.wispStatus;
    this.interval = 0;
    this.timeout = 0;
    this.timeoutCount = 15;
    this.wispError;
    this.isGet = false;
}
ap_router.prototype = {
    hidePlaceholder: function (id) {
        var doc = document.getElementById(id);
        this.del_space(doc.parentNode);
        doc.parentNode.firstChild.style.display = "none";
    },
    setPlaceholder: function (id) {
        var doc = document.getElementById(id);
        this.del_space(doc.parentNode);
        if (doc.value == "") {
            doc.parentNode.firstChild.style.display = "block";
        } else {
            doc.parentNode.firstChild.style.display = "none";
        }
    },
    del_space: function (elem) {
        var elem_child = elem.childNodes;
        for (var i = 0; i < elem_child.length; i++) {
            if (elem_child[i].nodeName == "#text") {
                elem.removeChild(elem_child[i]);
            }
        }
    },
    //登录验证
    checkLoginForm: function () {
        var pwd = document.getElementById("login_pwd");
        if (pwd.value == "") {
            alert("请输入密码!");
            return;
        }
        userLogin(pwd.value);
    },
    black_display: function () {
        ap_router.showLayer('layer_shade', 'expend_net');
    },
    black_list_confirm: function () {
        var mac = document.getElementById('black_mac');
        if (!ap_router.checkMac(mac.value)) {
            alert("MAC地址有误！");
            return;
        }
        edit_black_list(mac.value, '');
    },
    //手机侧滑栏
    sliderBarToggle: function () {
        var nav_toggle = document.getElementById("nav_toggle"),
                navIcon = document.getElementById("nav"),
                cover = document.getElementById("cover");
        this.toggleClass(nav_toggle, 'toggle-animate');
        if (this.hasClass(navIcon, "open-nav")) {
            this.removeClass(navIcon, "open-nav");
            this.addClass(navIcon, "close-nav");
        } else {
            this.addClass(navIcon, "open-nav");
            this.removeClass(navIcon, "close-nav");
        }
        if (this.hasClass(nav_toggle, "toggle-animate")) {
            var div = document.createElement("div");
            div.id = "cover";
            div.className = "cover";
            document.body.appendChild(div);
        } else {
            document.body.removeChild(cover);
        }
    },
    hasClass: function (obj, cls) {
        return obj.className.match(new RegExp('(\\s|^)' + cls + '(\\s|$)'));
    },
    addClass: function (obj, cls) {
        if (!this.hasClass(obj, cls))
            obj.className += " " + cls;
    },
    removeClass: function (obj, cls) {
        if (this.hasClass(obj, cls)) {
            var reg = new RegExp('(\\s|^)' + cls + '(\\s|$)');
            obj.className = obj.className.replace(reg, ' ');
        }
    },
    toggleClass: function (obj, cls) {
        if (this.hasClass(obj, cls)) {
            this.removeClass(obj, cls);
        } else {
            this.addClass(obj, cls);
        }
    },
    //弹出层
    showLayer: function (shadeId, layerId, content) {
        document.getElementById(shadeId).style.display = "block";
        document.getElementById(layerId).style.display = "block";
        if (content) {
            document.getElementById('content').innerHTML = content;
        }
    },
    hideLayer: function (shadeId, layerId) {
        document.getElementById(shadeId).style.display = "none";
        document.getElementById(layerId).style.display = "none";
    },
    setCookie: function (name, value) {
        document.cookie = name + "=" + escape(value) + "";
    },
    getCookie: function (name) {
        var arr, reg = new RegExp("(^| )" + name + "=([^;]*)(;|$)");
        if (arr = document.cookie.match(reg)) {
            return unescape(arr[2]);
        } else {
            return false;
        }
    },
    setByid: function (id, res, type) {
        if (type == 1) {
            return document.getElementById(id).value = res;
        } else {
            return document.getElementById(id).innerHTML = res;
        }
    },
    getByid: function (id, type) {
        if (type == 1) {
            return document.getElementById(id).value;
        } else {
            return document.getElementById(id).innerHTML;
        }
    },
    trim: function (str) {
        return str.replace(/(^\s*)|(\s*$)/g, '');
    },
    checkMac: function (mac) {
        mac = mac.toUpperCase();
        if (mac == "" || mac.indexOf(":") == -1) {
            return false;
        } else {
            var macs = mac.split(":");
            if (macs.length != 6) {
                return false;
            }
            for (var i = 0; i < macs.length; i++) {
                if (macs[i].length != 2) {
                    return false;
                }
            }
            var reg_name = /[A-F\d]{2}:[A-F\d]{2}:[A-F\d]{2}:[A-F\d]{2}:[A-F\d]{2}:[A-F\d]{2}/;
            if (!reg_name.test(mac)) {
                return false;
            }
        }
        return true;
    },
    ieTbodyHtml: function (tableId, html) {
        var docTable = document.getElementById(tableId);
        var newDiv = document.createElement('div');
        newDiv.innerHTML = '<table></tbody>' + html + '</tbody></table>'
        var newTbody = newDiv.childNodes[0].tBodies[0];
        var oldTbody = docTable.tBodies[0];
        docTable.replaceChild(newTbody, oldTbody);
    },
    isIE: function (ie_version) {
        var v = ie_version;
        var browser = navigator.appName;
        var b_version = navigator.appVersion;
        var version = b_version.split(";");
        if (version.length > 1) {
            var trim_Version = parseInt(version[1].replace(/[ ]/g, "").replace(/MSIE/g, ""));
            if (trim_Version <= v) {
                return false;
            }
        }
        return true;
    },
    getStrLength: function (str) {
        var realLength = 0, len = str.length, charCode = -1;
        for (var i = 0; i < len; i++) {
            charCode = str.charCodeAt(i);
            if (charCode > 0 && charCode <= 128) {
                realLength += 1;
            } else {
                realLength += 3;
            }
        }
        return realLength;
    },
    switch_open: function (op) {
        var enable = 'open';
        if (op == 1) {
            enable = '';
        } else if (op == 0) {
            enable = 'open';
        }
        return enable;
    },
    createXMLHttpRequest: function () {
        var XMLHttpReq;
        try {
            XMLHttpReq = new ActiveXObject("Msxml2.XMLHTTP");//IE高版本创建XMLHTTP
        } catch (E) {
            try {
                XMLHttpReq = new ActiveXObject("Microsoft.XMLHTTP");//IE低版本创建XMLHTTP
            } catch (E) {
                XMLHttpReq = new XMLHttpRequest();//兼容非IE浏览器，直接创建XMLHTTP对象
            }
        }
        return XMLHttpReq;
    },
    GetResponse: function (url) {
        var res = this.getAjaxReturn(url);
        if (res.error == 0) {
            return res;
        } else {
            if (res.opt == 'login') {
                return res;
            } else {
                this.locationUrl(res.error);
            }
        }
    },
    getAjaxReturn: function (url) {
        var xhr = this.createXMLHttpRequest();
        var data;
        xhr.open("POST", this.baseUrl + url, false);
        xhr.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
        xhr.onreadystatechange = function () {
            var XMLHttpReq = xhr;
            if (XMLHttpReq.readyState == 4) {
                if (XMLHttpReq.status == 200) {
                    var text = XMLHttpReq.responseText;
                    if (!ap_router.isIE(7)) {
                        text = eval("(" + text + ")");
                    } else {
                        text = JSON.parse(text);
                    }
                    data = text;
                }
            }
        };
        xhr.send();
        return data;
    },
    locationUrl: function (error) {
        if (error == 10007) {
            alert('请重新登录！');
            setTimeout(function () {
                //            $.cookie('lstatus', false, {path: '/'});
                document.location = 'http://' + document.domain + "/login.html?tt=" + new Date().getTime();
            }, 3000);
        } else {
            alert(getErrorCode(error));
        }
    },
    remove_black: function (index) {
        edit_black_list('', index, 'del');
    },
    connect_wisp: function (ssid, mac, channel, sec, ext) {
        var pwd = document.getElementById('have_pwd');
        if (sec == 'NONE') {
            pwd.style.display = 'none';
        } else {
            pwd.style.display = 'block';
        }
        ap_router.setByid('cyname', ssid);
        ap_router.setByid('channel', channel, 1);
        ap_router.setByid('data_mac', mac, 1);
        ap_router.setByid('data_ext', ext, 1);
        ap_router.setByid('data_sec', sec, 1);

        ap_router.showLayer('layer_shade', 'wifi_zj');
    },
    wifi_zj_confirm: function () {
        var mac = ap_router.getByid('data_mac', 1);
        var channel = ap_router.getByid('channel', 1);
        var ssid = ap_router.getByid('cyname');
        var ext = ap_router.getByid('data_ext', 1);
        var sec = ap_router.getByid('data_sec', 1);
        var pwd = ap_router.getByid('wf_pwd', 1);
        ssid = encodeURIComponent(ssid);
        if (sec != 'NONE') {
            if (pwd == '') {
                alert('请输入无线密码！');
                return;
            }
            if (pwd.length > 0 && pwd.length < 6) {
                alert('无线密码不能少于5位！');
                return;
            }
            if (escape(pwd).indexOf("%u") != -1 || /[\'\"{}\[\]]/.test(pwd)) {
                alert('无线密码不能包含中文字符或者特殊字符！');
                return;
            }
        }
        pwd = encodeURIComponent(pwd);
        if (pwd == null || pwd == '') {
            pwd = 'NONE';
            sec = 'NONE';
        }
        ap_router.hideLayer('layer_shade', 'wifi_zj');
        connectWisp(ssid, pwd, mac, channel, sec, ext);
    },
    wisp_hidden: function () {
        ap_router.showLayer('layer_shade', 'hidden_wisp');
    },
    check_sec: function () {
        var sec_check = document.getElementById('check_wpa').value;
        if (sec_check == 1) {
            document.getElementById('pwd_li').style.display = 'none';
        } else {
            document.getElementById('pwd_li').style.display = 'block';
        }
    },
    hidden_wisp_confirm: function () {
        var ssid = ap_router.getByid('ssid', 1);
        var pwd = 'NONE';
        var mac = ap_router.getByid('hidden_mac', 1);
        var channel = ap_router.getByid('check_channel', 1);
        var sec = ap_router.getByid('check_wpa', 1);
        if (ssid == '' || ssid == null) {
            alert('请输入无线名称！');
            return;
        }
        if (ap_router.getStrLength(ssid) > 31) {
            alert('无线名称不能超过31位！');
            return;
        }
        if (sec != 1) {
            pwd = ap_router.getByid('ssid_pwd', 1);
            if (pwd.length < 1) {
                alert('请输入无线密码！');
                return;
            }
        }
        if (pwd.length > 0 && pwd.length < 6 && pwd != 'NONE') {
            alert('无线密码不能少于6位！');
            return;
        }
        if (pwd.length > 0 && escape(pwd).indexOf("%u") != -1 || /[\'\"{}\[\]]/.test(pwd)) {
            alert('无线密码不能包含中文字符或者特殊字符！');
            return;
        }
        connectWisp(ssid, pwd, mac, channel, sec, 'NONE');
    }
}
var ap_router = new ap_router();
function getWanConfigJsonObject() {
    var url = 'fname=net&opt=ap_wan_conf&function=get';
    var res = ap_router.GetResponse(url);
//    res = {"mode": 2, "ssid": "wiair-wifi", "security": "WPA1PSKWPA2PSK/TKIPAES", "key": "123456789", "channel": "10", "bssid": "01:02:03:04:05:06", "extch": "NONE", "error": 0};
    return res;
}

function getWanInfoJsonObject() {
    var url = 'fname=net&opt=wan_info&function=get';
    var res = ap_router.GetResponse(url);
    return res;
}

function wanInfoResponse() {
    var res = getWanInfoJsonObject();
    if (res.error == 0) {
        wanInfoJsonObject = res;
        currentMode = wanInfoJsonObject.mode;
        if (res.reason > 0) {
            if (res.reason == 21) {
                wispError = 21;
            } else if (res.reason == 22) {
                wispError = 22;
            }
        }
        if (res.connected == 1) {
            if (ap_router.wispStatus == 1) {
                document.location = 'index.html';
            } else {
                ap_router.setByid('wanMac', res.mac);
                ap_router.setByid('wanIP', res.gateway);
                ap_router.setByid('tstatus', '拓展成功<span class="suc">✔</span>');
            }
        } else {
            ap_router.setByid('tstatus', '拓展失败，请重试<span class="wrong">✖</span>');
        }
    } else {
        locationUrl(res.error);
    }
}

function getWanAp() {
    var res = getWanConfigJsonObject();
    //console.log(res);
    ap_router.setByid('wifi_name', res.ssid);
    if (res.security == 'NONE') {
        ap_router.setByid('sec', '未加密');
    } else {
        ap_router.setByid('sec', res.security);
        ap_router.setByid('wifi_name', res.ssid);
    }
}

function getVersion() {
    var url = 'fname=net&opt=firmstatus&function=get';
    var res = ap_router.GetResponse(url);
    ap_router.setByid('version', res.localfirm_version);
}

function getWifiList() {
    var url = 'fname=net&opt=ap_list&function=get';
    ap_router.showLayer('layer_shade', 'loading');
    setTimeout(function () {
        var res = ap_router.GetResponse(url);
        wifiHtmlList(res);
    }, 1000);
}


function getWifiAp() {
    var url = 'fname=net&opt=wifi_ap&function=get';
    var res = ap_router.GetResponse(url);
    ap_router.setByid('wf_nameset', res.ssid, 1);
}
//重启
function routerRestart(act) {
    var url = "fname=system&opt=setting&action=" + act + "&function=set";
    ap_router.GetResponse(url);
    var content = '';
    //console.log(act);
    if (act == 'reboot') {
        content = "拓展器正在重新启动...";
    } else {
        content = "拓展器正在恢复出厂设置...";
    }
    ap_router.showLayer('layer_shade', 'expend_net', content);
    GetProgressBar("progressBar_box", "progressBar_Track");
}

//访客网络
function wifiGuestGet(act, type, vssid, pwd, hidden) {
    var dat = "fname=net&opt=wifi_vap&function=" + act;
    if (act == 'set') {
        dat = dat + "&enable=" + type;
        if (vssid != '' && vssid != undefined) {
            dat = dat + '&vssid=' + vssid;
        }
        if (pwd != '' && pwd != undefined) {
            dat = dat + '&password=' + pwd;
        }
        if (hidden != '' && hidden != undefined) {
            dat = dat + '&hidden=' + hidden;
        }
    }
    //console.log(dat);
    var res = ap_router.GetResponse(dat);
    var guest_on = document.getElementById('guest_on');
    var guest_off = document.getElementById('guest_off');
    var vset = document.getElementById('vset');
    //console.log(res);
    if (act == 'get' && res.enable == 1) {
        ap_router.addClass(guest_on, 'selected');
        ap_router.removeClass(guest_off, 'selected');
        vset.style.display = 'block';
        ap_router.setByid('wf_vnameset', res.vssid, 1);
        if (res.password != '' && res.security == 1) {
            ap_router.setByid('wf_vpwdset', res.password, 1);
        } else if (res.security == 0) {
            ap_router.setByid('wf_vpwdset', '', 1);
        }
        if (res.hidden == 1) {
            document.getElementById('hidden_vssid').checked = true;
        }
//        wifiGuestList('get');
    } else if (act == 'get' && res.enable == 0) {
        ap_router.addClass(guest_off, 'selected');
        ap_router.removeClass(guest_on, 'selected');
        vset.style.display = 'none';
    } else {
        alert('设置成功！');
        // console.log(ap_router);
        // console.log(act);
        // console.log(type);
        if (act == 'set' && type == 1) {
            wifiGuestGet('get');
//            wifiGuestList('get');
            ap_router.removeClass(guest_off, 'selected');
            ap_router.addClass(guest_on, 'selected');
            vset.style.display = 'block';
        } else if (act == 'set' && type == 0) {
            ap_router.addClass(guest_off, 'selected');
            ap_router.removeClass(guest_on, 'selected');
            vset.style.display = 'none';
        }
        if (type == 0) {
            //visit_mode_div.style.display = 'none';
        }
    }
}
function arrowGuest(obj) {
    var obj = obj,
            action = 'add',
            mac = obj.getAttribute('data').toUpperCase();
    //ap_router.toggleClass(obj, "open");
    if (obj.className == '') {
        obj.className = 'open';
        action = 'del';
        obj.innerHTML = '未授权';
    } else {
        obj.className = '';
        obj.innerHTML = '授权';
    }
    // if (!ap_router.hasClass(obj,'open')) {
    //     action = 'del';
    //     obj.innerHTML = '未授权';
    //     console.log(1);
    // }else{
    //     console.log(2);
    //      obj.innerHTML = '授权';
    // }
    wifiGuestList('set', mac, action);
}

function wifiGuestList(fun, mac, action) {
    var url = "fname=net&opt=vap_host&function=" + fun;
    if (mac != '' && fun == 'set') {
        url = "fname=net&opt=vap_host&function=" + fun + "&mac=" + mac + "&action=" + action;
    }
    var res = ap_router.GetResponse(url);
    if (fun == 'set') {
        alert("设置成功！");
    } else {
        var len = res.guests.length;
        var guestList = res.guests;
        var html = '';
        var switchClass = '';
        var guestName = '';
        if (len > 0) {
            for (var i = 0; i < len; i++) {
                guestName = guestList[i].name;
                switchClass = ap_router.switch_open(guestList[i].wlist);
                html += '<li class="v-name"><div class="name">' + guestName + '</div></li><li class="v-oper"><div class="tbDiv"><i onclick="arrowGuest(this);" class="' + switchClass + '" data=' + guestList[i].mac + '>未授权</i></div></li>';
            }
            ap_router.setByid('guestList', html);
            document.getElementById('guestList').innerHTML = html;
        } else {
        }
    }
}
function vwifi_set_confirm() {
    var vssid = document.getElementById("wf_vnameset").value;
    //console.log(vssid);
    var vpwd = document.getElementById("wf_vpwdset").value;
    var hidden = 0;
    if (document.getElementById("hidden_vssid").checked) {
        hidden = 1;
    }
    if (vssid == '') {
        alert("请输入无线名称！");
        return;
    }
    if (ap_router.getStrLength(vssid) > 31 || /[\':;*?~`!@#$%^&+={}\[\]\<\\(\),\.\。\，]/.test(vssid)) {
        alert("无线名称长度不得超过31位字符或者包含特殊字符！")
        return;
    }

    if (vpwd.length > 0) {
        if (vpwd.length > 31 || vpwd.length < 8) {
            alert("密码长度不能超过31位或不能少于8位！");
            return;
        }
        if (escape(vpwd).indexOf("%u") != -1 || /[\'\"{}\[\]]/.test(vpwd)) {
            alert("密码不能包含中文字符或者特殊字符！");
            return;
        }
    }
    vssid = encodeURIComponent(vssid);
    vpwd = encodeURIComponent(vpwd);
    wifiGuestGet('set', 1, vssid, vpwd, hidden);
}

//更改ap名称
function wifi_set_confirm() {
    var ssid = document.getElementById('wf_nameset').value;
    if (ssid == '') {
        alert('请输入无线名称！');
        return;
    }
    if (ap_router.getStrLength(ssid) > 31 || /[\':;*?~`!@#$%^&+={}\[\]\<\\(\),\.\。\，]/.test(ssid)) {
        alert('无线名称长度不得超过31位字符或者包含特殊字符！');
        return;
    }
    ssid = encodeURIComponent(ssid);
    setWifiAp(ssid);
}

//拓展网络
function connectWisp(ssid, pwd, mac, channel, sec, ext) {
    ap_router.showLayer('layer_shade', 'expend_net', '正在拓展...');
    if (sec == 1) {
        sec = 'NONE';
    } else if (sec != 'NONE') {
        var secsp = sec.split('/');
        if (secsp.length == 1) {
            sec = sec.replace(' ', '') + '/TKIPAES';
        }
    }
    var url = "fname=net&opt=ap_wan_conf&function=set" + "&bssid=" + mac + "&mode=2&ssid=" + ssid + "&security=" + sec + "&key=" + pwd + "&channel=" + channel + "&extch=" + ext;
    var res = ap_router.GetResponse(url);
    if (res.error == 0) {
        ap_router.wispStatus = 1;
        timeoutCount = 24;
        timeout = 0;
        interval = setInterval(function () {
            wanInfoResponse();
            if (responseReason == 19) {
                timeout = 24;
            }
            if (timeout > timeoutCount - 1) {
                ap_router.hideLayer('layer_shade', 'expend_net');
                alert('拓展失败:' + errorMessage(wispError));
                clearInterval(interval);
                ap_router.hideLayer('layer_shade', 'expend_net');
            }
            if (wanInfoJsonObject.connected == 0) {
                timeout++;
                if (wispError == 21 || wispError == 22) {
                    ap_router.setByid('content_message', getErrorCode(wispError));
                    ap_router.hideLayer('layer_shade', 'expend_net');
                }
            }
        }, 5000);
    } else {
        ap_router.hideLayer('layer_shade', 'expend_net');
        locationUrl(res.error);
    }
}

function edit_black_list(mac, index, method) {
    var tr = document.getElementById('black_list').childNodes, len = tr.length, macList = [], list = '', macLen = 0, maxNum = 32;
    if (method == 'del') {
        maxNum = 33;
    }
    if (len < maxNum) {
        for (var i = 0; i < len; i++) {
            if (index !== i) {
                macList.push(tr[i].firstChild.innerHTML);
            }
        }
        var str = isRepeat(macList, mac);
        if (str == false) {
            alert('重复添加！');
            return;
        }
        if (mac != '') {
            macList.push(mac);
        }
        macLen = macList.length;
        list = macList.join(';');
    }

    if (len >= 32 && method != 'del') {
        getMsg('最多能添加32组！');
        return;
    }
    add_black_list(list, macLen, mac, index);
}

function setUserAccount(pwd) {
    var url = 'fname=system&opt=login_account&function=set&user=admin&password=' + pwd;
    var res = ap_router.GetResponse(url);
    if (res.error == 0) {
        alert("修改成功,请用新密码重新登录！");
        setTimeout(function () {
            ap_router.setCookie('lstatus', false);
            document.location = 'http://' + document.domain + '/login.html?tt=' + new Date().getTime();
        }, 3000);
    }
}

function userLogin(str) {
    //var str_md5 = 'f6fdffe48c908deb0f4c3bd36c032e72f6fdffe48c908deb0f4c3bd36c032e72';
    var str_md5 = 'admin' + str;
//    str_md5 = 'f6fdffe48c908deb0f4c3bd36c032e72';
    var url = 'fname=system&opt=login&function=set&usrid=' + str_md5;
    var res = ap_router.getAjaxReturn(url);
    if (res.error == 0) {
        ap_router.setCookie('lstatus', true);
        document.location = 'index.html';
    } else {
        alert('密码错误！');
    }
}

//终端列表
function getDynamicInfo() {
    var url = 'fname=sys&opt=ap_host&function=get';
    var res = ap_router.GetResponse(url);
//   res = {"opt": "ap_host", "fname": "sys", "function": "get", "hosts": [{"iswifi": 0, "portid": 8, "ip": "192.168.9.115", "mac": "50:7b:9d:09:53:ae"}], "error": 0};
    var terminals = res.hosts;
    var html = "";
    var dynamicList = "";
    if (terminals != null && terminals.length > 0) {
        for (var i = 0; i < terminals.length; i++) {
            dynamicList += DynamicList(html, terminals[i].mac, terminals[i].ip);
        }
    }
    if (!ap_router.isIE(9)) {
        ap_router.ieTbodyHtml("client_tb", dynamicList);
    } else {
        ap_router.setByid('devices', dynamicList);
    }
}

//终端列表
function DynamicList(html, mac, ip) {
    html += '<tr mac="' + mac + '">';
    html += "<td>" + ip + "</td><td class='mb-hide'>" + mac + "</td><td><a href='javascript:;' onclick=devices_black('" + mac + "') class='link-btn black_add'>加入</a></td>";
    html += '</tr>';
    var list = html;
    return list;
}

function devices_black(mac) {
    edit_black_list(mac, '');
}

//黑名单列表
function black_list() {
    var url = 'fname=net&opt=acl_black&function=get&ifindex=0';
    var res = ap_router.GetResponse(url);
    //   res = {"opt": "acl_black", "fname": "net", "function": "get", "ifindex": "0", "mac": ["78:24:af:c0:b1:92", "78:24:af:c0:b7:93", "78:24:af:c0:b7:99"], "enable": 1, "error": 0};
    blackListHtml(res);
}

function add_black_list(list, len, mac, index) {
    var url = 'fname=net&opt=acl_black&function=set&ifindex=0&mac=' + list + '&mac_num=' + len + '&enable=1';
    var res = ap_router.GetResponse(url);
    if (res.error == 0) {
        alert('操作成功！');
        if (len > 0) {
            if (mac == '') {
                document.getElementById('black_list').removeChild[index];
            } else {
                var inde = len - 1;
                document.getElementById('black_list').appendChild('<tr><td>' + mac + '</td><td class="nobd"><a href="javascript:;" onclick=ap_router.remove_black(' + inde + ') class = "link-btn black_del" > 移除 < /a></td > < /tr>');
            }
        }
    }
}

function blackListHtml(data) {
    var len = data.mac.length, html = '';
    if (len > 0) {
        for (var i = 0; i < len; i++) {
            html += '<tr><td>' + data.mac[i] + '</td><td class="nobd"><a href="javascript:;" onclick=ap_router.remove_black(' + i + ') class="link-btn black_del">移除</a></td></tr>';
        }
    }
    if (!ap_router.isIE(9)) {
        ap_router.ieTbodyHtml("black_list_tb", html);
    } else {
        ap_router.setByid('black_list', html);
    }
}

function wifiHtmlList(jsonWifiListObject) {
    var aplist = jsonWifiListObject.aplist;
    var wifi = "";
    var list = "";
    var trclass;
    var data_ssid = new Array();
    if (aplist.length > 0) {
        for (var i = 0; i < aplist.length; i++) {
            if (i % 2 == 0) {
                trclass = 'even';
            } else {
                trclass = 'event';
            }
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
            if (!/script/.test(aplist[i].ssid)) {
                if (aplist[i].ssid.length > 15) {
                    data_ssid[i] = aplist[i].ssid.substr(0, 15) + '...';
                } else {
                    data_ssid[i] = aplist[i].ssid;
                }
                if (aplist[i].ssid.length >= 1 && ap_router.trim(aplist[i].ssid) != '') {
                    list = "<li onclick=ap_router.connect_wisp('" + aplist[i].ssid + "','" + aplist[i].bssid + "','" + aplist[i].channel + "','" + aplist[i].security + "','" + aplist[i].extch + "') class=" + trclass + "><div class='wDiv w-name' title='" + aplist[i].ssid + "'>" + data_ssid[i] + "</div><div class='wDiv w-mode'>" + aplist[i].security + "</div><div class='wDiv w-lvl'>" + aplist[i].security_lock + "</div></li>";
                    wifi += list;
                }
            }
        }
    }

    ap_router.setByid('getWifiList', wifi);
    ap_router.hideLayer('layer_shade', 'loading');
}


//设置wifi名称
function setWifiAp(name) {
    var url = 'fname=net&opt=wifi_ap&function=set&ssid=' + name;
    ap_router.GetResponse(url);
    alert('设置成功！');
}

function checkConfig() {
    var lstatus = ap_router.getCookie('lstatus');
    if (lstatus == true) {
        document.location = 'index.html';
    } else {
        document.location = 'login.html';
    }
}

function getErrorCode(type) {
    switch (type) {
        case 21 :
            return "正在获取IP...";
            break;
        case 22 :
            return "已获取IP...";
            break;
        case 10001:
            return "获取参数失败！";
            break;
        case 10002:
            return "输入参数错误！";
            break;
        case 10003:
            return "路由内存不足！";
            break;
        case 10004:
            return "添加的条目已存在！";
            break;
        case 10005:
            return "删除的条目不存在！";
            break;
        case 10006:
            return "添加条目已满！";
            break;
        case 10007:
            return "没登录！";
            break;
        case 10008:
            return "不支持！";
            break;
        case 10010:
            return "时间过期！";
            break;
        case 20100000:
            return "固件暂不支持！";
            break;
        default:
            return "请求超时，错误码：" + type;
    }
}

//进度条
function GetProgressBar(boxId, trackId) {
    var ProgressBar = {
        maxValue: 100,
        value: 0,
        SetValue: function (aValue) {
            this.value = aValue;
            if (this.value >= this.maxValue)
                this.value = this.maxValue;
            if (this.value <= 0)
                this.value = 0;
            var w = document.getElementById(boxId).offsetWidth;
            var mWidth = this.value / this.maxValue * w;
            document.getElementById(trackId).style.width = mWidth + "px";
        }
    }
    ProgressBar.maxValue = 100;
    var index = 0;
    var mProgressTimer = setInterval(function () {
        index += 0.83;
        if (index > 99) {
            window.clearInterval(mProgressTimer);
            ProgressBar.SetValue(0);
            ap_router.hideLayer('layer_shade', 'expend_net');
        }
        ProgressBar.SetValue(index.toFixed(2));
    }, 1000);
    setTimeout(function () {
        ap_router.setCookie("lstatus", false);
        document.location = 'http://' + document.domain + '/index.html?tt=' + new Date().getTime();
    }, 120000);
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
                document.getElementById("_submit").value = "上传中...";
                document.getElementById("_submit").disabled = true;
                document.getElementById("msg").innerHTML = "正在上传，请稍候...";
                document.getElementById("form1").action = "/upload.csp?uploadpath=/tmp/ioos&t=firmware.bin";
                document.getElementById("form1").submit();
                ap_router.interval = setInterval(function () {
                    if (ap_router.timeout > ap_router.timeoutCount - 1) {
                        ap_router.timeout = 0;
                        clearInterval(ap_router.interval);
                        document.getElementById("msg").innerHTML = "超时，上传失败！";
                        document.getElementById("_submit").value = "确认上传";
                        document.getElementById("_submit").disabled = false;
                    }
                    if (!ap_router.isGet) {
                        getResponseTimer();
                        ap_router.timeout++;
                    }
                }, 4000);
            } else {
                locationUrl(status);
            }
        }
    }
}

function upReady() {
    var url = "fname=system&opt=up_ready&function=set&math=" + Math.random();
    var res = ap_router.GetResponse(url);
    var status = 0;
    if (res.error == 0) {
        status = 0;
    } else {
        status = res.error;
    }
    return status;
}

function getResponseTimer() {
    var url = "fname=net&opt=upload_status&function=get&math=" + Math.random();
    var res = ap_router.GetResponse(url);
    if (res.error == 0) {
        if (res.status == 1) {
            clearInterval(ap_router.interval);
            ap_router.isGet = true;
            ap_router.timeout = 0;
            setUpgrade();
        }
    } else {
        locationUrl(res.error);
    }
}

function toLogin() {
    ap.router.setCookie("lstatus", false);
    document.location = 'http://' + document.domain + "/index.html?tt=" + new Date().getTime();
}

function setUpgrade() {
    var url = "fname=system&opt=local_firmup&function=set";
    var res = ap_router.GetResponse(url);
    if (res.error == 0) {
        document.getElementById("progressBar").style.display = "block";
        GetProgressBar("progressBar", "progressBar_Track2");
        document.getElementById("_submit").value = "升级中...";
        document.getElementById("_submit").disabled = true;
        document.getElementById("msg").innerHTML = "正在升级，大约需要3分钟。升级过程中请勿操作或断电...";
    } else {
        document.getElementById("_submit").value = "重新上传";
        ap_router.interval = 0;
        ap_router.isGet = false;
        document.getElementById("_submit").disabled = false;
        document.getElementById("msg").innerHTML = "升级遇到错误，请联系客服！错误码：" + res.error;
    }
}

//修改密码
function user_confirm() {
    var oldpwd = document.getElementById('old_pwd').value;
    if (oldpwd == '') {
        alert('请输入旧密码！');
        return false;
    }
    var name = 'admin';
    var str_md5 = name + oldpwd;
//    str_md5 = 'f6fdffe48c908deb0f4c3bd36c032e72';
    var url = "fname=system&opt=login&function=set&usrid=" + str_md5;
    var res = ap_router.GetResponse(url);
    var flag = false;
    if (res.error == 0) {
        flag = true;
    }
    if (flag == false) {
        alert('旧密码验证错误！');
        return false;
    }
    var pwd = document.getElementById('userpwd').value;
    var pwd2 = document.getElementById('new_pwd2').value;
    if (pwd == '') {
        alert('请输入密码！');
        return false;
    }
    if (pwd2 != pwd) {
        alert('两次密码不一致！');
        return false;
    }

    if (oldpwd == pwd) {
        alert('新密码和旧密码相同！');
        return false;
    }

    if (pwd.length > 15 || pwd.length < 5) {
        alert('密码长度不能超过15位或者小于5位！');
        return false;
    }

    if (escape(pwd).indexOf("%u") != -1 || /[\':;*?~`!@#$%^&+={}\[\]\<\\(\),\.\。\，]/.test(pwd)) {
        alert('密码不能包含中文字符或者特殊字符！');
        return false;
    }
    setUserAccount(pwd);
}

function isRepeat(list, mac) {
    var bool = true;
    for (var i = 0; i < list.length; i++) {
        if (list[i] == mac) {
            bool = false;
            break;
        }
    }
    return bool;
}
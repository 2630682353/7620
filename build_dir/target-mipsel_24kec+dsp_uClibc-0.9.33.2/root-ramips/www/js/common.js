var intervalTimerCount = 0;
var commonInterval = 4000;
var common_total_speed = 0;
var common_speed_step = 100;
var common_speed_min = 5;
var ledStatus = false;
var interval_flag = false;
var regexp = RegExp(/[\':;*?~`!@#$%^&+={}\[\]\<\\(\),\.\。\，]/);
var MODEL = new Array("其它", "华为", "小米", "魅族", "磊科", "联想", "长虹", "HTC", "苹果", "三星", "诺基亚", "索尼", "酷派", "OPPO", "LG", "中兴", "步步高VIVO", "摩托罗拉", "金立", "天语", "TCL", "飞利浦", "戴尔", "惠普", "华硕", "东芝", "宏碁", "海信", "腾达", "TPLINK", "一加", "锤子", "瑞昱", "清华同方", "触云", "创维", "康佳", "乐视", "海尔", "夏普", "VMware", "Intel", "技嘉", "海华", "其它", "其它", "其它", "其它", "仁宝", "其它", "糖葫芦", "其它", "其它", "谷歌", "天珑", "泛泰", "其它", "其它", "微软", "极路由", "奇虎360", "水星", "华勤", "安利", "富士施乐", "D-Link", "网件", "阿里巴巴", "微星", "TCT", "莱蒙", "高科", "Nexus", "朵唯", "小辣椒", "语信", "欧博信", "爱立顺", "蓝魔", "海美迪", "优尔得", "夏新", "金讯宏盛", "E派", "同洲", "海客", "天迈通讯", "尚锋", "蓝博兴", "斐讯", "七彩虹", "酷鸽", "欧新", "佳域", "艾优尼", "亿通", "唯乐", "大成", "酷比魔方", "本为", "国虹", "VINUS", "强者", "爱可视", "迈乐", "汇科", "富可视", "先锋", "奥克斯", "波导", "邦华", "神舟", "宏为", "英特奇", "酷宝", "欧比", "昂达", "卓酷", "高新奇", "广信", "酷比", "酷博", "维图", "飞秒", "果米", "米歌", "米蓝", "诺亚信", "糯米", "腾信时代", "小霸王", "华星", "UBTEL", "neken", "真米", "MTK", "思科", "必联");
var actionUrl = document.domain;
var mtd_param;
if (actionUrl != null && actionUrl != "") {
    actionUrl = "http://" + actionUrl + "/protocol.csp?";
} else {
    alert("获取路由器网关IP失败！");
}

var browser = {
    versions: function () {
        var u = navigator.userAgent, app = navigator.appVersion;
        return {
            trident: u.indexOf('Trident') > -1, //IE内核 
            presto: u.indexOf('Presto') > -1, //opera内核 
            webKit: u.indexOf('AppleWebKit') > -1, //苹果、谷歌内核 
            gecko: u.indexOf('Gecko') > -1 && u.indexOf('KHTML') == -1, //火狐内核 
            mobile: !!u.match(/AppleWebKit.*Mobile.*/), //是否为移动终端 
            ios: !!u.match(/\(i[^;]+;( U;)? CPU.+Mac OS X/), //ios终端 
            android: u.indexOf('Android') > -1 || u.indexOf('Linux') > -1, //android终端或uc浏览器 
            iPhone: u.indexOf('iPhone') > -1, //是否为iPhone或者QQHD浏览器 
            iPad: u.indexOf('iPad') > -1, //是否iPad 
            webApp: u.indexOf('Safari') == -1 //是否web应该程序，没有头部与底部 
        };
    }(),
    language: (navigator.browserLanguage || navigator.language).toLowerCase()
}

/*  
 * MAP对象，实现MAP功能  
 */
function Map() {
    this.elements = new Array();

    //获取MAP元素个数   
    this.size = function () {
        return this.elements.length;
    }

    //判断MAP是否为空   
    this.isEmpty = function () {
        return(this.elements.length < 1);
    }

    //删除MAP所有元素   
    this.clear = function () {
        this.elements = new Array();
    }

    //向MAP中增加元素（key, value)    
    this.put = function (_key, _value) {
        this.elements.push({
            key: _key,
            value: _value
        });
    }

    //删除指定KEY的元素，成功返回True，失败返回False   
    this.remove = function (_key) {
        var bln = false;
        try {
            for (i = 0; i < this.elements.length; i++) {
                if (this.elements[i].key == _key) {
                    this.elements.splice(i, 1);
                    return true;
                }
            }
        } catch (e) {
            bln = false;
        }
        return bln;
    }

    //获取指定KEY的元素值VALUE，失败返回NULL   
    this.get = function (_key) {
        try {
            for (i = 0; i < this.elements.length; i++) {
                if (this.elements[i].key == _key) {
                    return this.elements[i].value;
                }
            }
        } catch (e) {
            return null;
        }
    }

    //获取指定索引的元素（使用element.key，element.value获取KEY和VALUE），失败返回NULL   
    this.element = function (_index) {
        if (_index < 0 || _index >= this.elements.length) {
            return null;
        }
        return this.elements[_index];
    }

    //判断MAP中是否含有指定KEY的元素   
    this.containsKey = function (_key) {
        varbln = false;
        try {
            for (i = 0; i < this.elements.length; i++) {
                if (this.elements[i].key == _key) {
                    bln = true;
                }
            }
        } catch (e) {
            bln = false;
        }
        return bln;
    }

    //判断MAP中是否含有指定VALUE的元素   
    this.containsValue = function (_value) {
        var bln = false;
        try {
            for (i = 0; i < this.elements.length; i++) {
                if (this.elements[i].value == _value) {
                    bln = true;
                }
            }
        } catch (e) {
            bln = false;
        }
        return bln;
    }

    //获取MAP中所有VALUE的数组（ARRAY）   
    this.values = function () {
        var arr = new Array();
        for (i = 0; i < this.elements.length; i++) {
            arr.push(this.elements[i].value);
        }
        return arr;
    }

    //获取MAP中所有KEY的数组（ARRAY）   
    this.keys = function () {
        var arr = new Array();
        for (i = 0; i < this.elements.length; i++) {
            arr.push(this.elements[i].key);
        }
        return arr;
    }
}

var terminal_speed_map = new Map();

/*
 * 去重字符串两边空格
 */
function trim(str) {
    regExp1 = /^ */;
    regExp2 = / *$/;
    return str.replace(regExp1, '').replace(regExp2, '');
}
/*
 * 验证MAC
 */
function checkMac(mac) {
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
}

/*
 * 获取URL参数值
 */
function getQueryString(name) {
    var reg = new RegExp("(^|&)" + name + "=([^&]*)(&|$)");
    var r = window.location.search.substr(1).match(reg);
    if (r != null) {
        return unescape(r[2]);
    }
    return null;
}

/*
 * 检查IP是否合法
 */
function checkIP(ip) {
    obj = ip;
    var exp = /^(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])$/;
    var reg = obj.match(exp);
    if (reg == null) {
        return false;
    } else {
        return true;
    }
}
/*
 * 检查掩码是否合法
 */
function checkMask(mask) {
    obj = mask;
    var exp = /^(254|252|248|240|224|192|128)\.0\.0\.0|255\.(254|252|248|240|224|192|128|0)\.0\.0|255\.255\.(254|252|248|240|224|192|128|0)\.0|255\.255\.255\.(254|252|248|240|224|192|128|0)$/;
    var reg = obj.match(exp);
    if (reg == null) {
        return false;
    } else {
        return true;
    }
}

/*
 * 功能：实现IP地址，子网掩码，网关的规则验证
 * 参数：IP地址，子网掩码，网关
 * 返回值：BOOL
 */
var validateNetwork = function (ip, netmask, gateway) {
    var parseIp = function (ip) {
        return ip.split(".");
    }
    var conv = function (num) {
        var num = parseInt(num).toString(2);
        while ((8 - num.length) > 0)
            num = "0" + num;
        return num;
    }
    var bitOpera = function (ip1, ip2) {
        var result = '', binaryIp1 = '', binaryIp2 = '';
        for (var i = 0; i < 4; i++) {
            if (i != 0)
                result += ".";
            for (var j = 0; j < 8; j++) {
                result += conv(parseIp(ip1)[i]).substr(j, 1) & conv(parseIp(ip2)[i]).substr(j, 1)
            }
        }
        return result;
    }
    var ip_re = /^(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[1-9])\.(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[1-9]|0)\.(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[1-9]|0)\.(25[0-5]|2[0-4][0-9]|[0-1]{1}[0-9]{2}|[1-9]{1}[0-9]{1}|[0-9])$/;
    if (ip == null || netmask == null || gateway == null) {
        return false;
    }
    if (!ip_re.test(ip) || !ip_re.test(netmask) || !ip_re.test(gateway)) {
        return false
    }
    return bitOpera(ip, netmask) == bitOpera(netmask, gateway);
}

/**
 * [isEqualIPAddress 判断两个IP地址是否在同一个网段]
 * @param  {[String]}  addr1 [地址一]
 * @param  {[String]}  addr2 [地址二]
 * @param  {[String]}  mask  [子网掩码]
 * @return {Boolean}         [true or false]
 */
function isEqualIPAddress(addr1, addr2, mask) {
    if (!addr1 || !addr2 || !mask) {
//        getMsg("各参数不能为空");
        return false;
    }
    var res1 = [], res2 = [];
    addr1 = addr1.split(".");
    addr2 = addr2.split(".");
    mask = mask.split(".");
    for (var i = 0, ilen = addr1.length; i < ilen; i += 1) {
        res1.push(parseInt(addr1[i]) & parseInt(mask[i]));
        res2.push(parseInt(addr2[i]) & parseInt(mask[i]));
    }
    if (res1.join(".") == res2.join(".")) {
        return true;
    } else {
        return false;
    }
}

/*
 * 获取错误码
 */
function getErrorCode(type) {
    switch (type) {
        case 19 :
            return "宽带账号/密码错误！";
            break;
        case 20 :
            return "运营商拨号无响应！";
            break;
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
            return"添加条目已满！";
            break;
        case 10007:
            return "没登陆！";
            break;
        case 10008:
            return "不支持！";
            break;
        case 10009:
            return"没获取到账号！";
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

function errorMessage(code) {
    switch (code) {
        case 0:
            return "连接超时";
            break;
        case 19 :
            return "宽带账号/密码错误！";
            break;
        case 20:
            return "运营商无响应";
            break;
        case 21:
            return "获取IP失败！";
            break;
        case 22:
            return "IP获取成功，无法上网";
            break;
        default:
            return "连接超时";
    }
}

function typeButton(model) {
    switch (model) {
        case 1:
            return 'dhcp';
            break;
        case 2:
            return 'pppoe';
            break;
        case 3:
            return 'static';
            break;
        case 5:
            return '3g';
            break;
    }
}

/*
 * 转换tcp/udp
 */
function changeTcpUdp(tu) {
    tu = parseInt(tu);
    switch (tu) {
        case 1:
            return 'TCP';
            break;
        case 2:
            return 'UDP';
            break;
        default:
            return 'TCP/UDP';
    }
}

function changeTuNum(tu) {
    switch (tu) {
        case 'TCP':
            return 1;
            break;
        case 'UDP':
            return 2;
            break;
        default:
            return 3;
    }
}

//转换open/close
function switch_open(op, num) {
    var enable = 'switch' + num;
    if (op == 1) {
        if (mob == 1) {
            enable = 'swichBtn';
        } else {
            enable = 'switch' + num + ' open' + num;
        }
    } else if (op == 0) {
        if (mob == 1) {
            enable = 'swichBtn close-btn';
        } else {
            enable = 'switch' + num;
        }
    }
    return enable;
}


//网络配置四种模式显示
function netTypeChoice(type) {
    $("#netWorkSet").show().siblings(".wrap").hide();
    if (type == 1) {
        $("#t_3").addClass('selected');
        $("#t_1").removeClass('selected');
        $("#t_2").removeClass('selected');
        $("#t_4").removeClass('selected');
        $("#t_3_box").show();
        $("#t_1_box").hide();
        $("#t_2_box").hide();
        $("#t_4_box").hide();
        showDHCP();
        showPPOE();
        showStatic();
    } else if (type == 2 || type == 0) {
        $("#t_1").addClass('selected');
        $("#t_2").removeClass('selected');
        $("#t_3").removeClass('selected');
        $("#t_4").removeClass('selected');
        $("#t_1_box").show();
        $("#t_2_box").hide();
        $("#t_3_box").hide();
        $("#t_4_box").hide();
        if (wanConfigJsonObject.user != '' && typeof (wanConfigJsonObject.user) != 'undefined') {
            showPPOE();
        }
        showDHCP();
        showStatic();
    } else if (type == 3) {
        $("#t_2").addClass('selected');
        $("#t_1").removeClass('selected');
        $("#t_3").removeClass('selected');
        $("#t_4").removeClass('selected');
        $("#t_1_box").hide();
        $("#t_3_box").hide();
        $("#t_2_box").show();
        $("#t_4_box").hide();
        if (wanConfigJsonObject.ip != '' && typeof (wanConfigJsonObject.ip) != 'undefined') {
            showStatic();
        }
        showDHCP();
        showPPOE();
    } else if (type == 4) {
        $("#t_4").addClass('selected');
        $("#t_1").removeClass('selected');
        $("#t_2").removeClass('selected');
        $("#t_3").removeClass('selected');
        $("#t_1_box").hide();
        $("#t_2_box").hide();
        $("#t_3_box").hide();
        $("#t_4_box").show();
        $("#mac_panel").hide();
        getWifiList();
        showPPOE();
        showStatic();
        showDHCP();
    }
}

function showPPOE() {
    if (wanConfigJsonObject.user != '' && typeof (wanConfigJsonObject.user) != 'undefined') {
        $("#acc").val(wanConfigJsonObject.user).siblings("label").hide();
        $("#pwd").val(wanConfigJsonObject.passwd).siblings("label").hide();
        if (trim(wanConfigJsonObject.pppoe_server) != '') {
            $("#pppoe_server").val(wanConfigJsonObject.pppoe_server).siblings("label").hide();
        }
        $("#pp_mtu").val(wanConfigJsonObject.mtu_pppoe).siblings("label").hide();
        if (trim(wanConfigJsonObject.dns_pppoe) != '') {
            $("#pp_dns").val(wanConfigJsonObject.dns_pppoe).siblings("label").hide();
        }
        if (trim(wanConfigJsonObject.dns1_pppoe) != '') {
            $("#pp_dns1").val(wanConfigJsonObject.dns1_pppoe).siblings("label").hide();
        }
        $("#mac_box1").text(wanConfigJsonObject.mac);
        if (wanConfigJsonObject.mac == wanConfigJsonObject.hostmac) {
            $("#macChoose1 option[value=2]").attr('selected', 'selected');
        } else if (wanConfigJsonObject.mac == wanConfigJsonObject.rawmac) {
            $("#macChoose1 option[value=3]").attr('selected', 'selected');
        } else {
            $("#mac_box1").hide();
            $("#macChoose1 option[value=1]").attr('selected', 'selected');
            $("#macEnter1").val(wanConfigJsonObject.mac).parent('span').show();
        }
    }
    var li = $("#t_1_box ul").find('li');
    if (wanConfigJsonObject.dns_pppoe == '' && wanConfigJsonObject.dns1_pppoe == '' && wanConfigJsonObject.pppoe_server == '') {
        li.each(function (index) {
            if (index > 1) {
                $(this).hide();
                $("#PpoeHightSet").text('高级配置');
            }
        });
    } else {
        li.each(function (index) {
            if (index > 1) {
                $(this).show();
                $("#PpoeHightSet").text('简单配置');
            }
        });
    }
}

function showDHCP() {
    if (trim(wanConfigJsonObject.dns_dhcp) != '') {
        $("#dhcp_dns").val(wanConfigJsonObject.dns_dhcp).siblings("label").hide();
    }
    if (trim(wanConfigJsonObject.dns1_dhcp) != '') {
        $("#dhcp_dns1").val(wanConfigJsonObject.dns1_dhcp).siblings("label").hide();
    }
    $("#dhcp_mtu").val(wanConfigJsonObject.mtu_dhcp).siblings("label").hide();
    $("#mac_box3").text(wanConfigJsonObject.mac);
    if (wanConfigJsonObject.mac == wanConfigJsonObject.hostmac) {
        $("#macChoose3 option[value=2]").attr('selected', 'selected');
    } else if (wanConfigJsonObject.mac == wanConfigJsonObject.rawmac) {
        $("#macChoose3 option[value=3]").attr('selected', 'selected');
    } else {
        $("#mac_box3").hide();
        $("#macChoose3 option[value=1]").attr('selected', 'selected');
        $("#macEnter3").val(wanConfigJsonObject.mac).parent('span').show();
    }

    var li = $("#t_3_box ul").find('li');
    if (wanConfigJsonObject.dns_dhcp == '' && wanConfigJsonObject.dns1_dhcp == '') {
        li.each(function () {
            $(this).hide();
            $("#DhcpHightSet").text('高级配置');
        });
    } else {
        li.each(function () {
            $(this).show();
            $("#DhcpHightSet").text('简单配置');
        });
    }
}

function showStatic() {
    if (wanConfigJsonObject.ip != '' && typeof (wanConfigJsonObject.ip) != 'undefined') {
        $("#static_ip").val(wanConfigJsonObject.ip).siblings("label").hide();
        $("#static_mask").val(wanConfigJsonObject.mask).siblings("label").hide();
        $("#static_gw").val(wanConfigJsonObject.gw).siblings("label").hide();
        if (trim(wanConfigJsonObject.dns_static) != '') {
            $("#static_dns").val(wanConfigJsonObject.dns_static).siblings("label").hide();
        }
        if (trim(wanConfigJsonObject.dns1_static) != '') {
            $("#static_dns1").val(wanConfigJsonObject.dns1_static).siblings("label").hide();
        }
        $("#static_mtu").val(wanConfigJsonObject.mtu_static).siblings("label").hide();

        $("#mac_box2").text(wanConfigJsonObject.mac);
        if (wanConfigJsonObject.mac == wanConfigJsonObject.hostmac) {
            $("#macChoose2 option[value=2]").attr('selected', 'selected');
        } else if (wanConfigJsonObject.mac == wanConfigJsonObject.rawmac) {
            $("#macChoose2 option[value=3]").attr('selected', 'selected');
        } else {
            $("#mac_box2").hide();
            $("#macChoose2 option[value=1]").attr('selected', 'selected');
            $("#macEnter2").val(wanConfigJsonObject.mac).parent('span').show();
        }

        myStaticIp = wanConfigJsonObject.ip;
        myStaticMask = wanConfigJsonObject.mask;
        myStaticGw = wanConfigJsonObject.gw;
        myStaticDns = wanConfigJsonObject.dns_static;
    }
    var li = $("#t_2_box ul").find('li');
    if (wanConfigJsonObject.dns_static == '' && wanConfigJsonObject.dns1_static == '') {
        li.each(function (index) {
            if (index > 2) {
                $(this).hide();
                $("#StaticHightSet").text('高级配置');
            }
        });
    } else {
        li.each(function (index) {
            if (index > 2) {
                $(this).show();
                $("#StaticHightSet").text('简单配置');
            }
        });
    }
}

function toUrl() {
    if (browser.versions.android || browser.versions.iPhone || browser.versions.iPad) {
        document.location = 'http://' + document.domain + "/m/user/index.html?tt=" + new Date().getTime();
    } else {
        document.location = 'http://' + document.domain + "/user/index.html?tt=" + new Date().getTime();
    }
}

function uniencode(text)
{
    text = escape(text.toString()).replace(/\+/g, "%u2");
    var matches = text.match(/(%([0-9A-F]{2}))/gi);
    if (matches) {
        for (var matchid = 0; matchid < matches.length; matchid++) {
            var code = matches[matchid].substring(1, 3);
            if (parseInt(code, 16) >= 128) {
                text = text.replace(matches[matchid], '%u00' + code);
            }
        }
    }
    text = text.replace('%25', '%u0025');
    return text;
}


/*
 * 检测是否是手机
 */
function getMobile() {
    var mob = 0;
    if (browser.versions.mobile || browser.versions.android || browser.versions.iPhone) {
        mob = 1;
    }
    return mob;
}

function getMsg(msg, type, sid) {
    var mob = getMobile();
    if (mob == 1) {
        return tips(msg);
    } else if (type == 1 && mob != 1) {
        layer.tips(msg, sid, {
            tips: [1, '#000000']
        });
    } else {
        return layer.msg(msg);
    }
}

/*
 * 设置wifi账户密码
 */
function setWifiAp(ssid, pwd, channel, hidden_ssid, wifiBandwidth) {
    var dat = "fname=net&opt=wifi_ap&function=set&ssid=" + ssid + "&passwd=" + pwd + "&hidden=" + hidden_ssid + "&channel=" + channel + "&bw=" + wifiBandwidth;
    $.ajax({
        type: "POST",
        url: actionUrl + dat,
        dataType: "json",
        success: function (data) {
            if (data.error == 0) {
                getMsg('设置成功！');
                $("#wifi_name").text(decodeURIComponent(ssid));
                $("#wifi_password").text(pwd);
            } else {
                locationUrl(data.error);
            }
        }
    });
}

/*
 设置终端昵称
 */
function setTerminalNick(mac, nick) {
    var macTmp = mac.replace(/:/g, "");
    var macNick = $("#h_nick_" + macTmp).val();
    //alert(encodeURIComponent(macNick) + "   " + nick);
    if (encodeURIComponent(macNick) != nick) {
        $.ajax({
            type: "POST",
            url: actionUrl + "fname=net&opt=host_nick&function=set&mac=" + mac + "&nick=" + nick + "&math=" + Math.random(),
            dataType: "json",
            success: function (data) {
                if (data.error == 0) {
                } else {
                    locationUrl(data.error);
                }
            }
        });
    }
}


/*
 设置终端网络禁用
 */
function setTerminalForbidden(mac, act) {
    $.ajax({
        type: "POST",
        url: actionUrl + "fname=net&opt=host_black&function=set&mac=" + mac + "&act=" + act,
        dataType: "json",
        success: function (data) {
            if (data.error == 0) {
            } else {
                locationUrl(data.error);
            }
        }
    });
}


/*
 设置终端上传下载速度
 */
function setTerminalSpeed(mac, down_speed, up_speed) {
    var macTmp = mac.replace(/:/g, "");
    var macDownSpeed = $("#h_d_" + macTmp).val() == "不限速" ? "0" : $("#h_d_" + macTmp).val();
    var macUpSpeed = $("#h_u_" + macTmp).val() == "不限速" ? "0" : $("#h_u_" + macTmp).val();
    //alert(macDownSpeed + "   " + down_speed + "   -----------" + macUpSpeed + "   " + up_speed);
    if (macDownSpeed != down_speed || macUpSpeed != up_speed) {
        $.ajax({
            type: "POST",
            url: actionUrl + "fname=net&opt=host_ls&function=set&mac=" + mac + "&speed=" + down_speed + "&up_speed=" + up_speed + "&math=" + Math.random(),
            dataType: "json",
            success: function (data) {
                if (data.error == 0) {
                } else {
                    locationUrl(data.error);
                }
            }
        });
    }
}

function getStrLength(str) {
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
}


function getLedLogin() {
    $.ajax({
        type: "POST",
        url: actionUrl + "fname=system&opt=led&function=set&act=dump&math=" + Math.random(),
        async: false,
        dataType: "json",
        success: function (data) {
            if (data.error == 0) {
                ledStatus = true;
            } else {
                ledStatus = false;
            }
        }, error: function () {
            ledStatus = false;
        }
    });
    return ledStatus;
}

function rLogin() {
    var rl = false;
    $.ajax({
        type: "POST",
        url: actionUrl + "fname=system&opt=login&function=set&usrid=f6fdffe48c908deb0f4c3bd36c032e72",
        async: false,
        dataType: "json",
        success: function (loginJson) {
            if (loginJson.error == 0) {
                rl = true;
            } else {
                rl = false;
            }
        }, error: function () {
            rl = false;
        }
    });
    return rl;
}

/*
 * 终端列表
 */

function getClientList() {
    $.ajax({
        type: "POST",
        url: actionUrl + "fname=system&opt=main&function=get",
        dataType: "json",
        success: function (data) {
            var jsonObject = data;
            if (jsonObject.error == 0) {
                var terminals = jsonObject.terminals;
                var html = '';
                var html2 = '';
                var mac_mark = '';
                if (terminals != null && terminals.length > 0) {
                    for (var i = 0; i < terminals.length; i++) {
                        var model = "";
                        if (MODEL != null) {
                            if (MODEL[terminals[i].vendor] === undefined) {
                                MODEL[terminals[i].vendor] == '其它';
                            } else {
                                model = MODEL[terminals[i].vendor] + "-";
                            }
                        }
                        var device_name;
                        if (terminals[i].flag.substr(6, 1) == 'T') {
                            device_name = terminals[i].name;
                        } else if (terminals[i].vendor == 34) {
                            mac_mark = terminals[i].mac.replace(/:/g, "");
                            device_name = '触云-' + mac_mark.substr(-4, 4);
                        } else {
                            device_name = model + terminals[i].name;
                        }

                        if (terminals[i].flag.substr(1, 1) == 'T') {
                            html += "<option ip=" + terminals[i].ip + " mac=" + terminals[i].mac + ">" + device_name + "</option>";
                            if (i == 0) {
                                hostNatList('get', terminals[i].mac, 'dump');
                            }
                        }

                        if (terminals[i].flag.substr(1, 1) == 'F') {
                            html2 += "<option mac=" + terminals[i].mac + ">" + device_name + "</option>";
                        }
                    }
                }
                $("#client_list").html(html + html2);
            } else {
                locationUrl(data.error);
            }
        }
    });
}


/*
 * 秒转时分秒
 */
function formatSeconds(value) {
    var theTime = parseInt(value);// 秒
    var theTime1 = 0;// 分
    var theTime2 = 0;// 小时
    // alert(theTime);
    if (theTime > 60) {
        theTime1 = parseInt(theTime / 60);
        theTime = parseInt(theTime % 60);
        // alert(theTime1+"-"+theTime);
        if (theTime1 > 60) {
            theTime2 = parseInt(theTime1 / 60);
            theTime1 = parseInt(theTime1 % 60);
        }
    }
    var result = "" + parseInt(theTime) + "秒";
    if (theTime1 > 0) {
        result = "" + parseInt(theTime1) + "分" + result;
    }
    if (theTime2 > 0) {
        result = "" + parseInt(theTime2) + "小时" + result;
    }
    return result;
}

function sortOnLineList(obj) {
    var len = obj.length;
    for (var i = 0; i < len; i++) {
        if (obj[i].flag.substr(0, 1) == 'T') {
            obj[i] = 0;
        }
        console.log(obj[i].flag);
        if (obj[i].flag.substr(1, 1) == 'F') {
            obj[i] = obj[i] + 1;
        }
    }
}

function down(x, y) {
    return (x.ip.length < y.ip.length) ? 1 : -1;
}

var router = {
    //获取系统版本
    getVersion: function () {
        $.ajax({
            type: "POST",
            url: actionUrl + "fname=system&opt=firmstatus&function=get&flag=local&math=" + Math.random(),
            dataType: "json",
            success: function (data) {
                var jsonObject = data;
                if (jsonObject.error == 0) {
                    $("#firmware_model").text(jsonObject.model);
                    if (jsonObject.born !== undefined) {
                        jsonObject.born = jsonObject.born.replace(/-/g, '').replace(/:/, '').replace(/ /, '');
                        jsonObject.localfirm_version = jsonObject.localfirm_version + ' (' + jsonObject.born + ')';
                    }
                    $("#version").html(jsonObject.localfirm_version);
                } else {
                    $("#version").html(getErrorCode(data.error));
                }
            }, complete: function (XHR, TS) {
                XHR.abort();
            }, error: function (XHRequest, status, data) {
                XHRequest.abort();
            }
        });
    },
    //获取网络类型
    getWanInfo: function () {
        $.ajax({
            type: "POST",
            url: actionUrl + "fname=net&opt=wan_info&function=get&math=" + Math.random(),
            dataType: "json",
            success: function (data) {
                var jsonObject = data;
                if (jsonObject.error == 0) {
                    $("#netType").text(getWanNetType(jsonObject.mode));
                    $("#wanIP").text(jsonObject.ip);
                    $('#macTy').text(jsonObject.mac);
                    $("#dns1").text(jsonObject.DNS1);
                    $("#dns2").text(jsonObject.DNS2);
                    $("#mask").text(jsonObject.mask);
                    $("#gw").text(jsonObject.gateway);
                    if (jsonObject.connected == 0) {
                        $(".state-img").addClass('failed');
                    }
                } else {
                    $("#netType").html("获取网络类型失败！");
                    $("#wanIP").html("获取外网IP失败！");
                }
            }, complete: function (XHR, TS) {
                XHR.abort();
            }, error: function (XHRequest, status, data) {
                XHRequest.abort();
            }
        });
    },
    //获取model.js
    getModel: function () {
        $.getScript('http://static.wiair.com/conf/model.js', function () {
            if (typeof (M_BRAND) != 'undefined') {
                MODEL = M_BRAND;
            }
        });
    },
    //获取LAN IP
    getLanIP: function () {
        $.ajax({
            type: "POST",
            url: actionUrl + "fname=net&opt=dhcpd&function=get&math=" + Math.random(),
            dataType: "json",
            success: function (data) {
                var jsonObject = data;
                if (jsonObject.error == 0) {
                    $("#lanIP").html(jsonObject.ip);
                } else {
                    $("#lanIP").html(getErrorCode(data.error));
                }
            }, complete: function (XHR, TS) {
                XHR.abort();
            }, error: function (XHRequest, status, data) {
                XHRequest.abort();
            }
        });
    },
    //获取QOS信息
    getQos: function () {
        $.ajax({
            type: "POST",
            url: actionUrl,
            data: "fname=net&opt=qos&function=get&math=" + Math.random(),
            dataType: "json",
            success: function (data) {
                var jsonObject = data;
                if (jsonObject.error == 0) {
                    if (jsonObject.enable == 0) {
                        $("#totalSpeed").html("未知");
                    } else {
                        $("#totalSpeed").html((jsonObject.down / 1024).toFixed(0) + "Mb");
                    }
                }
            }
        });
    },
    //获取终端限速
    getTerminalLimitSpeed: function () {
        terminal_speed_map = new Map();
        $.ajax({
            type: "POST",
            url: actionUrl + "fname=system&opt=host_if&function=get&if=FFFFFFFT&math=" + Math.random(),
            async: false,
            dataType: "json",
            success: function (data) {
                var jsonObject = data;
                if (jsonObject.error == 0) {
                    var terminals = jsonObject.terminals;
                    if (terminals != null && terminals.length > 0) {
                        for (var i = 0; i < terminals.length; i++) {
                            var flag = terminals[i].flag;
                            //if (flag.charCodeAt(1) == 84) {	//T
                            terminal_speed_map.put(terminals[i].mac, terminals[i].ls + ":" + terminals[i].ls_up);
                            // }
                        }
                    }
                }
            }, complete: function (XHR, TS) {
                XHR = null;
            }, error: function (XHRequest, status, data) {
                XHRequest.abort();


            }
        });
    },
    //获取路由器动态信息
    getDynamicInfo: function () {
        router.getTerminalLimitSpeed();
        $.ajax({
            type: "POST",
            url: actionUrl + "fname=system&opt=main&function=get&math=" + Math.random(),
            dataType: "json",
            success: function (data) {
                var jsonObject = data;
                if (jsonObject.error == 0) {
                    common_total_speed = jsonObject.total_speed;
                    if (jsonObject.connected == true) {
                        $("#netStatus").html('已连接');
                        $(".state-img").removeClass('failed');
                    } else {
                        $("#netStatus").html('正在连接...');
                        $(".state-img").addClass('failed');
                    }
                    $("#ontime").text(formatSeconds(jsonObject.ontime));
                    if (jsonObject.cur_speed > 1024) {
                        if (mob == 1) {
                            $("#curSpeed").html((jsonObject.cur_speed / 1024).toFixed(2));
                            $("#downspeedType").html('M/S');
                        } else {
                            $("#curSpeed").html((jsonObject.cur_speed / 1024).toFixed(2) + "<em>M/S</em>");
                        }
                    } else {
                        if (mob == 1) {
                            $("#curSpeed").html(jsonObject.cur_speed);
                            $("#downspeedType").html('KB/S');
                        } else {
                            $("#curSpeed").html(jsonObject.cur_speed + "<em>KB/S</em>");
                        }
                    }

                    if (jsonObject.up_speed > 1024) {
                        if (mob == 1) {
                            $("#up_speed").html((jsonObject.up_speed / 1024).toFixed(2));
                            $("#upspeedType").html('M/S');
                        } else {
                            $("#up_speed").html((jsonObject.up_speed / 1024).toFixed(2) + "<em>M/S</em>");
                        }
                    } else {
                        if (mob == 1) {
                            $("#up_speed").html(jsonObject.up_speed);
                            $("#upspeedType").html('KB/S');
                        } else {
                            $("#up_speed").html(jsonObject.up_speed + "<em>KB/S</em>");
                        }
                    }
                    var terminals = jsonObject.terminals;
                    var html = "";
                    var html2 = "", dynamicList = "";
                    var sort = 0;
                    var switch_flag = "";
                    var wifi_flag = "";
                    var t_count = 0;
                    var mac_mark = '';
                    if (terminals != null && terminals.length > 0) {
                        for (var i = 0; i < terminals.length; i++) {
                            var flag = terminals[i].flag;
                            if (flag.charCodeAt(1) == 84) {	//T
                                t_count++;
                                sort++;
                            } else {
                                terminals[i].ip = '已离线';
                            }
                            var model = "";
                            if (MODEL != null) {
                                if (MODEL[terminals[i].vendor] === undefined) {
                                    MODEL[terminals[i].vendor] == '其它';
                                } else {
                                    model = MODEL[terminals[i].vendor] + "-";
                                }
                            }
                            if (terminals[i].speed > 1024) {
                                terminals[i].speed = (terminals[i].speed / 1024).toFixed(2) + 'MB/S';
                            } else {
                                terminals[i].speed = terminals[i].speed + 'KB/S';
                            }

                            if (terminals[i].up_speed > 1024) {
                                terminals[i].up_speed = (terminals[i].up_speed / 1024).toFixed(2) + 'MB/S';
                            } else {
                                terminals[i].up_speed = terminals[i].up_speed + 'KB/S';
                            }
                            var device_name;
                            if (terminals[i].name == '') {
                                terminals[i].name = terminals[i].mac.replace(/:/g, "").substr(-4, 4);
                            }
                            if (terminals[i].flag.substr(6, 1) == 'T') {
                                device_name = terminals[i].name;
                            } else if (terminals[i].vendor == 34) {
                                mac_mark = terminals[i].mac.replace(/:/g, "");
                                device_name = '触云-' + mac_mark.substr(-4, 4);
                            } else {
                                device_name = model + terminals[i].name;
                            }

                            if (terminals[i].flag.substr(0, 1) == 'T') {
                                $("#main_mac").val(terminals[i].mac);
                            }
                            if (terminals[i].flag.substr(2, 1) == 'F') {
                                if (mob == 1) {
                                    switch_flag = 'swichBtn client-swich';
                                } else {
                                    switch_flag = 'switch open';
                                }
                            } else {
                                if (mob == 1) {
                                    switch_flag = 'swichBtn client-swich close-btn';
                                } else {
                                    switch_flag = 'switch';
                                }
                            }
                            if (flag.charCodeAt(9) == 84) {	//T
                                wifi_flag = "";
                                if (mob == 1) {
                                    wifi_flag = "2";
                                }
                            } else {
                                wifi_flag = "wifiOpen";
                                if (mob == 1) {
                                    wifi_flag = "1";
                                }
                            }
                            var tmp_down_speed = "0";
                            var tmp_up_speed = "0";
                            if (terminal_speed_map != null) {
                                if (terminal_speed_map.get(terminals[i].mac) != null) {
                                    tmp_down_speed = (terminal_speed_map.get(terminals[i].mac)).split(":")[0];
                                    tmp_up_speed = (terminal_speed_map.get(terminals[i].mac)).split(":")[1];
                                }
                                tmp_down_speed = tmp_down_speed == "0" ? "不限速" : tmp_down_speed;
                                tmp_up_speed = tmp_up_speed == "0" ? "不限速" : tmp_up_speed;
                            }
                            dynamicList += DynamicList(html, html2, terminals[i].flag.substr(1, 1), terminals[i].mac, device_name, terminals[i].ip, terminals[i].speed, terminals[i].up_speed, tmp_down_speed, tmp_up_speed, switch_flag, wifi_flag);
                        }
                    }
                    $("#count_devices").html(sort);
                    $("#terminal_count").html(t_count);
                    $("#devices").html(dynamicList);

                    //名称
                    $(".edit").on("click", function () {
                        if (!interval_flag) {
                            clearInterval(intervalTimerCount);
                            interval_flag = true;
                        }
                        var cname = $(this).parents("tr").find(".name")
                        if (mob == 1) {
                            cname = $(this).parent().find(".client-t");
                        }
                        cname.children("input").removeAttr("readonly").removeClass("nameInpt").select();
                    });
                    var rname = $(".name");
                    if (mob == 1) {
                        rname = $(".client-t");
                    }
                    rname.find("input.tbInpt").on("focusout", function () {
                        $(this).attr("readonly", "readonly");
                        $(this).addClass("nameInpt");
                        var terminal_mac = $(this).parents("tr").attr("mac");
                        if (mob == 1) {
                            terminal_mac = $(this).parents("ul").attr("mac");
                        }
                        var terminal_nick = $(this).val();
                        var v = $(this).siblings("input").val();
                        if (interval_flag) {
                            if (getStrLength(trim(terminal_nick)) < 1) {
                                getMsg('终端名称不能为空！');
                                terminal_nick = v;
                            }

                            if (getStrLength(terminal_nick) > 31) {
                                getMsg('终端名称过长！');
                                terminal_nick = v;
                            }

                            if (/[\':;*?~`!@#$%^&+={}\[\]\<\\(\),\.\。\，]/.test(terminal_nick)) {
                                getMsg('终端名称不能包含特殊字符！');
                                terminal_nick = v;
                            }

                            terminal_nick = encodeURIComponent(terminal_nick);

                            setTerminalNick(terminal_mac, terminal_nick);
                            if (interval_flag) {
                                intervalTimerCount = setInterval(function () {
                                    router.getDynamicInfo();
                                }, commonInterval);
                                interval_flag = false;
                            }
                        }
                    });

                    //开关
                    var swtich = $(".switch");
                    if (mob == 1) {
                        swtich = $(".swichBtn");
                    }
                    swtich.on("click", function () {
                        clearInterval(intervalTimerCount);
                        if (mob == 1) {
                            $(this).toggleClass("close-btn");
                        } else {
                            $(this).toggleClass("open");
                        }
                        var switch_act = "on";
                        var terminal_mac = $(this).parents("tr").attr("mac");
                        if ($(this).hasClass("open")) {
                            switch_act = "off";
                        } else {
                            switch_act = "on";
                        }

                        if (mob == 1) {
                            terminal_mac = $(this).parents("ul").attr("mac");
                            if ($(this).hasClass('close-btn')) {
                                switch_act = "on";
                            } else {
                                switch_act = "off";
                            }
                        }

                        setTerminalForbidden(terminal_mac, switch_act);
                        intervalTimerCount = setInterval(function () {
                            router.getDynamicInfo();
                        }, commonInterval);
                    });

                    //输入框限速
                    var regx = /^[1-9]\d*$/;
                    $(".limit").on("focusin", function () {
                        clearInterval(intervalTimerCount);
                        $(this).select();
                    }).on("focusout", function () {
                        //var mySpeed = "";
                        var terminal_mac = $(this).parents("tr").attr("mac");
                        if (mob == 1) {
                            terminal_mac = $(this).parents("ul").attr("mac");
                        }
                        var ds_val, us_val;

                        if (!regx.exec($(this).val())) {
                            $(this).val("不限速");
                            $(this).parent().siblings(".progress").slider("value", common_total_speed);
                        } else if ($(this).val() <= 0 || $(this).val() >= (common_total_speed - common_speed_step - common_speed_min)) {
                            $(this).val("不限速");
                            $(this).parent().siblings(".progress").slider("value", common_total_speed);
                        } else {
                            var v = $(this).val();
                            if (v != '') {
                                $(this).val(v);
                                $(this).parent().siblings(".progress").slider("value", v);
                            }
                        }
                        if ($(this).attr("id").indexOf("ds") >= 0) {
                            ds_val = $(this).val();
                            us_val = $(this).parents("tr").find(".usLimit").val();
                            if (mob == 1) {
                                us_val = $(this).parents("ul").find(".usLimit").val();
                            }
                        }
                        if ($(this).attr("id").indexOf("us") >= 0) {
                            us_val = $(this).val();
                            ds_val = $(this).parents("tr").find(".dsLimit").val();
                            if (mob == 1) {
                                ds_val = $(this).parents("ul").find(".dsLimit").val();
                            }
                        }
                        if (ds_val == "不限速") {
                            ds_val = 0;
                        }
                        if (us_val == "不限速") {
                            us_val = 0;
                        }
                        //alert(ds_val +"         "+us_val);
                        setTerminalSpeed(terminal_mac, ds_val, us_val);
                        intervalTimerCount = setInterval(function () {
                            router.getDynamicInfo();
                        }, commonInterval);
                    })
                    //拖动条限速
                    var mapArray = terminal_speed_map.keys();
                    if (mapArray != null && mapArray.length > 0) {
                        var ds_val, us_val;
                        //alert("mapArray = " + mapArray);
                        for (var m = 0; m < mapArray.length; m++) {
                            for (var i = 1; i <= 2; i++) {
                                var mySpeed = 0;
                                if (i == 2) {
                                    mySpeed = (terminal_speed_map.get(mapArray[m])).split(":")[1];
                                } else {
                                    mySpeed = (terminal_speed_map.get(mapArray[m])).split(":")[0];
                                }
                                if (mySpeed == 0) {
                                    mySpeed = common_total_speed;
                                }
                                if (mob != 1) {
                                    $("#slider_" + mapArray[m].replace(/:/g, "") + "_" + i).slider({
                                        range: "min",
                                        step: common_speed_step,
                                        value: mySpeed,
                                        min: common_speed_min,
                                        max: common_total_speed,
                                        slide: function (event, ui) {
                                            clearInterval(intervalTimerCount);
                                            if (ui.value >= (common_total_speed - common_speed_step - common_speed_min)) {
                                                $(this).siblings().find(".limit").val("不限速");
                                            } else {
                                                $(this).siblings().find(".limit").val(ui.value);
                                            }
                                        },
                                        stop: function (event, ui) {
                                            clearInterval(intervalTimerCount);
                                            var _this = $(this).siblings().find(".limit");
                                            if (_this.attr("id").indexOf("ds") >= 0) {
                                                ds_val = _this.val();
                                                us_val = _this.parents("tr").find(".usLimit").val();
                                            }
                                            if (_this.attr("id").indexOf("us") >= 0) {
                                                us_val = _this.val();
                                                ds_val = _this.parents("tr").find(".dsLimit").val();
                                            }
                                            if (ds_val == "不限速") {
                                                ds_val = 0;
                                            }
                                            if (us_val == "不限速") {
                                                us_val = 0;
                                            }
                                            var terminal_mac = $(this).parents("tr").attr("mac");
                                            setTerminalSpeed(terminal_mac, ds_val, us_val);
                                            intervalTimerCount = setInterval(function () {
                                                router.getDynamicInfo();
                                            }, commonInterval);
                                        }
                                    });
                                }
                            }
                        }
                    }
                } else {
                    $("#devices").html(getErrorCode(data.error));
                }
            },
            complete: function (XHR, TS) {
                XHR = null;
            },
            error: function (XHRequest, status, data) {
                XHRequest.abort();


            }
        });
    },
    //获取设备SSID
    getWifiAp: function () {
        $.ajax({
            type: "POST",
            url: actionUrl + 'fname=net&opt=wifi_ap&function=get&math=' + Math.random(),
            dataType: 'JSON',
            success: function (data) {
                var jsonObject = data;
                if (jsonObject.error == 0) {
                    $("#channel").val(jsonObject.channel);
                    if (jsonObject.hidden == 1) {
                        $("#hidden_ssid").attr("checked", true);
                    }
                    $('#wifi_name').text(jsonObject.ssid);
                    $('#wifi_name2').text(jsonObject.ssid);
                    $('#wf_nameset').val(jsonObject.ssid).siblings("label").hide();
                    if (trim(jsonObject.passwd) != '') {
                        $('#wf_pwdset').val(jsonObject.passwd).siblings("label").hide();
                    }
                    $("#wifiChannel option[value=" + jsonObject.channel + "]").attr('selected', 'selected');
                    $("#wifiBandwidth option[value=" + jsonObject.bw + "]").attr('selected', 'selected');
                }
            }, complete: function (XHR, TS) {
                XHR.abort();
            }, error: function (XHRequest, status, data) {
                XHRequest.abort();
            }
        });
    },
    getLedInfo: function () {
        $.ajax({
            type: "POST",
            url: actionUrl + 'fname=system&opt=led&function=set&act=dump',
            dataType: 'JSON',
            success: function (data) {
                var jsonObject = data;
                if (jsonObject.error == 0) {
                    if (jsonObject.led == 1) {
                        $("#led_on").addClass('selected');
                    } else if (jsonObject.led == 0) {
                        $("#led_off").addClass('selected');
                    }
                }
            }
        });
    },
    getBusinessPic: function () {
        $.ajax({
            type: "POST",
            url: actionUrl + "fname=sys&opt=mtd_param&function=get&valname=plat",
            dataType: "JSON",
            success: function (data) {
                var jsonObject = data;
                if (jsonObject.error == 0) {
                    if (jsonObject.value == 1) {
                        $("#showBusinessPic").show();
                        $("#businessPic").attr('src', '../images/erweima.png');
                    } else if (jsonObject.value == 2) {
                        $("#showBusinessPic").show();
                        $("#businessPic").attr('src', '../images/erweima2.png');
                    }
                }
            }
        });
    }
}
//获取登录信息
function getLoginInfo() {
    $.ajax({
        type: "POST",
        url: actionUrl + "fname=system&opt=led&function=set&act=dump&math=" + Math.random(),
        dataType: 'JSON',
        success: function (data) {
            var jsonObject = data;
            if (jsonObject.error != 0) {
                document.location = 'http://' + document.domain;
            }
        }
    });
}

//获取网络类型字符串
function getWanNetType(type) {
    switch (type) {
        case 1 :
            return "动态IP";
            break;
        case 2 :
            return "PPPOE";
            break;
        case 3 :
            return "静态IP";
            break;
        case 4 :
            return "WISP";
            break;
        case 5:
            return '3G';
            break;
        default :
            return "获取网络类型失败";
    }
}

//终端列表控制
function DynamicList(html, html2, flag, mac, name, ip, speed, up_speed, tmp_down_speed, tmp_up_speed, switch_flag, wifi_flag) {
    var mob = getMobile();
    if (flag == 'T') {
        if (mob == 1) {
            html += '<ul class="client-list bgfff bd" mac="' + mac + '">';
            html += "<li style='width:30%'><div class='client-t'><input type='hidden' id='h_nick_" + mac.replace(/:/g, "") + "' value='" + name + "' title='" + name + "'><input type='text' readonly='readonly' class='tbInpt nameInpt' value='" + name + "' title='" + name + "'></div>";
            html += '<div class="client-b"><span class="icon"><img src="../../images/m/c-' + wifi_flag + '.png"></span>' + ip + '</div></li>';
            html += '<li style="width:10%" class="edit"><span class="icon"><img src="../../images/m/c-3.png"></span></li>';
            html += '<li style="width:40%"><div class="client-t"><span>' + speed + '</span> <span>' + up_speed + '</span></div>';
            html += "<div class='client-b'><input type='hidden' id='h_d_" + mac.replace(/:/g, "") + "' value='" + tmp_down_speed + "'><input type='text' id='ds_" + i + "' class='tbInpt2 limit dsLimit' value='" + tmp_down_speed + "'>";
            html += " <input type='hidden' id='h_u_" + mac.replace(/:/g, "") + "' value='" + tmp_up_speed + "'><input type='text' id='us_" + i + "' class='tbInpt2 limit usLimit' value='" + tmp_up_speed + "'></div></li>";
            html += '<li style="width:20%"><a href="javascript:;" class="' + switch_flag + '"></a></li>';
            html += '</ul>';
        } else {
            html += '<tr mac="' + mac + '">';
            html += "<td><div class='tbDiv'><span class='name'><input type='hidden' id='h_nick_" + mac.replace(/:/g, "") + "' value='" + name + "' title='" + name + "'><input type='text' readonly='readonly' class='tbInpt nameInpt' value='" + name + "' title='" + name + "'></span><span class='small'><i class='tbicon-1 " + wifi_flag + "'></i>" + ip + "</span></div></td>";
            html += "<td width='60'><div class='tbDiv'><i class='tbicon-2 edit'></i></div></td>";
            html += "<td><div class='tbDiv'><i class='tbicon-3'></i>" + speed + "</div></td>";
            html += "<td><div class='tbDiv'><i class='tbicon-4'></i>" + up_speed + "</div></td>";
            html += "<td><div class='tbDiv'><span><input type='hidden' id='h_d_" + mac.replace(/:/g, "") + "' value='" + tmp_down_speed + "'><input type='text' id='ds_" + i + "' class='tbInpt limit dsLimit' value='" + tmp_down_speed + "'></span><div class='progress' id='slider_" + mac.replace(/:/g, "") + "_1'></div></div></td>";
            html += "<td><div class='tbDiv'><span><input type='hidden' id='h_u_" + mac.replace(/:/g, "") + "' value='" + tmp_up_speed + "'><input type='text' id='us_" + i + "' class='tbInpt limit usLimit' value='" + tmp_up_speed + "'></span><div class='progress' id='slider_" + mac.replace(/:/g, "") + "_2'></div></div></td>";
            html += "<td><div class='tbDiv'><i class='" + switch_flag + "'></i></div></td>";
            html += '</tr>';
        }
    }

    if (flag == 'F') {
        if (mob == 1) {
            html2 += '<ul class="client-list bgfff bd" mac="' + mac + '">';
            html2 += "<li style='width:30%'><div class='client-t'><input type='hidden' id='h_nick_" + mac.replace(/:/g, "") + "' value='" + name + "' title='" + name + "'><input type='text' readonly='readonly' class='tbInpt nameInpt' value='" + name + "' title='" + name + "'></div>";
            html2 += '<div class="client-b"><span class="icon"><img src="../../images/m/c-' + wifi_flag + '.png"></span>' + ip + '</div></li>';
            html2 += '<li style="width:10%" class="edit"><span class="icon"><img src="../../images/m/c-3.png"></span></li>';
            html2 += '<li style="width:40%"><div class="client-t"><span>' + speed + '</span> <span>' + up_speed + '</span></div>';
            html2 += "<div class='client-b'><input type='hidden' id='h_d_" + mac.replace(/:/g, "") + "' value='" + tmp_down_speed + "'><input type='text' id='ds_" + i + "' class='tbInpt2 limit dsLimit' value='" + tmp_down_speed + "'>";
            html2 += " <input type='hidden' id='h_u_" + mac.replace(/:/g, "") + "' value='" + tmp_up_speed + "'><input type='text' id='us_" + i + "' class='tbInpt2 limit usLimit' value='" + tmp_up_speed + "'></div></li>";
            html2 += '<li style="width:20%"><a href="javascript:;" class="' + switch_flag + '"></a></li>';
            html2 += '</ul>';
        } else {
            html2 += '<tr mac="' + mac + '">';
            html2 += "<td><div class='tbDiv'><span class='name'><input type='hidden' id='h_nick_" + mac.replace(/:/g, "") + "' value='" + name + "' title='" + name + "'><input type='text' readonly='readonly' class='tbInpt nameInpt' value='" + name + "' title='" + name + "'></span><span class='small'><i class='tbicon-1 " + wifi_flag + "'></i>" + ip + "</span></div></td>";
            html2 += "<td width='60'><div class='tbDiv'><i class='tbicon-2 edit'></i></div></td>";
            html2 += "<td><div class='tbDiv'><i class='tbicon-3'></i>" + speed + "</div></td>";
            html2 += "<td><div class='tbDiv'><i class='tbicon-4'></i>" + up_speed + "</div></td>";
            html2 += "<td><div class='tbDiv'><span><input type='hidden' id='h_d_" + mac.replace(/:/g, "") + "' value='" + tmp_down_speed + "'><input type='text' id='ds_" + i + "' class='tbInpt limit dsLimit' value='" + tmp_down_speed + "'></span><div class='progress' id='slider_" + mac.replace(/:/g, "") + "_1'></div></div></td>";
            html2 += "<td><div class='tbDiv'><span><input type='hidden' id='h_u_" + mac.replace(/:/g, "") + "' value='" + tmp_up_speed + "'><input type='text' id='us_" + i + "' class='tbInpt limit usLimit' value='" + tmp_up_speed + "'></span><div class='progress' id='slider_" + mac.replace(/:/g, "") + "_2'></div></div></td>";
            html2 += "<td><div class='tbDiv'><i class='" + switch_flag + "'></i></div></td>";
            html2 += "<td><div class='tbDiv'><div class='editBtn remove'></div></div></td>";
            html2 += '</tr>';
        }
    }
    var list = html + html2;
    return list;
}

//错误消息提示
function locationUrl(error) {
    if (error == 10007) {
        getMsg('请重新登录！');
        setTimeout(function () {
            $.cookie('lstatus', false, {path: '/'});
            $.cookie('lstatus', false, {path: '/m'});
            document.location = 'http://' + document.domain + "/index.html?tt=" + new Date().getTime();
        }, 3000);
    } else {
        getMsg(getErrorCode(error));
    }
}

function getLocalTime(datetime) {
    var time = new Date(datetime * 1000);
    var year = time.getFullYear();
    var month = (time.getMonth() + 1) > 9 && (time.getMonth() + 1) || ('0' + (time.getMonth() + 1))
    var date = time.getDate() > 9 && time.getDate() || ('0' + time.getDate())
    var hour = time.getHours() > 9 && time.getHours() || ('0' + time.getHours())
    var minute = time.getMinutes() > 9 && time.getMinutes() || ('0' + time.getMinutes())
    var second = time.getSeconds() > 9 && time.getSeconds() || ('0' + time.getSeconds())
    var YmdHis = year + '-' + month + '-' + date
            + ' ' + hour + ':' + minute + ':' + second;
    return YmdHis;
}

//获取Router Info
function init() {
//    router.getUserLogin();
    router.getDynamicInfo();
    router.getModel();
    router.getVersion();
    router.getLanIP();
    router.getQos();
    router.getWifiAp();
//    router.getLedInfo();
    router.getWanInfo();
    router.getTerminalLimitSpeed();
//    router.getBusinessPic();
    intervalTimerCount = setInterval(function () {
        router.getDynamicInfo();
    }, commonInterval);
}
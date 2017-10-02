$(function () {
    $("#gw_on").click(function () {
        GwInfo('set', 1);
    });

    $("#gw_off").click(function () {
        GwInfo('set', 0);
    });

    $("#vwifi_set_confirm").click(function () {
        var vssid = trim($("#wf_vnameset").val());
        var vpwd = trim($("#wf_vpwdset").val());
        var hidden = 0;
        if ($("#hidden_vssid").attr('checked')) {
            hidden = 1;
        }
        if (vssid == '') {
            getMsg('请输入无线名称！', 1, '#wf_vnameset');
            return;
        }
        if (getStrLength(vssid) > 31 || /[\':;*?~`!@#$%^&+={}\[\]\<\\(\),\.\。\，]/.test(vssid)) {
            getMsg('无线名称长度不得超过31位字符或者包含特殊字符！', 1, '#wf_nameset');
            return;
        }

        if (vpwd.length > 0) {
            if (vpwd.length > 31 || vpwd.length < 8) {
                getMsg('密码长度不能超过31位或不能少于8位！', 1, '#wf_vpwdset');
                return;
            }
            if (escape(vpwd).indexOf("%u") != -1 || /[\'\"{}\[\]]/.test(vpwd)) {
                getMsg('密码不能包含中文字符或者特殊字符！', 1, '#wf_vpwdset');
                return;
            }
        }
        vssid = encodeURIComponent(vssid);
        vpwd = encodeURIComponent(vpwd);
        wifiGuestGet('set', 1, vssid, vpwd, hidden);
    });
});

function GwInfo(type, val) {
    var dataUrl = 'fname=sys&opt=telcom&function=' + type;
    if (type == 'set') {
        dataUrl = dataUrl + '&disabled=' + val;
    }
    $.post(actionUrl + dataUrl, function (data) {
        if (data.error == 0) {
            if (type == 'set') {
                getMsg('设置成功！');
            } else {
                if (data.disabled == 1) {
                    if ($("#gw_on").hasClass('selected') == false) {
                        $("#gw_on").addClass('selected');
                    }
                    if ($("#gw_off").hasClass('selected') == true) {
                        $("#gw_off").removeClass('selected');
                    }
                    if (mob == 1 && $("#routerGw").hasClass('selected') == false) {
                        $("#routerGw").addClass('close-btn');
                    }
                } else {
                    if (mob == 1) {
                        $("#routerGw").removeClass('close-btn');
                    } else {
                        if ($("#gw_on").hasClass('selected') == true) {
                            $("#gw_on").removeClass('selected');
                        }
                        if ($("#gw_off").hasClass('selected') == false) {
                            $("#gw_off").addClass('selected');
                        }
                    }
                }
            }
        } else {
            locationUrl(data.error);
        }
    }, 'json');
}

function black_list(type) {
    var html = '', len = 0, cls = 'pink';
    if (type == 1) {
        cls = 'pink2'
    }
    $.post(actionUrl + 'fname=net&opt=acl_black&function=get&ifindex=' + type, function (data) {
        if (data.error == 0) {
            len = data.mac.length;
            if (len > 0) {
                if (mob==0){
                    for (var i = 0; i < len; i++) {
                        html += '<tr class="even"><td width="250">' + data.mac[i] + '</td><td class="nobd"><a href="javascript:;" class="' + cls + '">取消黑名单</a></td></tr>';
                    }
                }else{
                    for (var i = 0; i < len; i++) {
                        html += '<li ><div>' + data.mac[i] + '</div><div><a href="javascript:;" class="mbpink">取消黑名单</a></div></li>';
                    }           
                }
            }

            if (html == '') {
                if(mob==0){
                    html = '<tr class="liL"><td colspan="3"><font size="+2">暂无内容</font></td></tr>';
                }else{
                    html = '<li class="liL"><h3>暂无内容</h3></li>';
                }
            }

            if (type == 0) {
                $("#backList").empty().html(html);
            } else {
                $("#backList2").empty().html(html);
            }

        } else {
            locationUrl(data.error);
        }
    }, 'json');
}

function add_black_list(list, len, mac, index, type) {

    var cls = 'pink',
        liHtml = '<tr class="liL"><td colspan="3"><font size="+2">暂无内容</font></td></tr>',
        mobliHtml = '<li class="liL"><h3>暂无内容</h3></li>';
    
    if (type == 1) {
        cls = 'pink2';
    }
    $.ajaxSettings.async = false;
    $.post(actionUrl + 'fname=net&opt=acl_black&function=set&ifindex=' + type + '&mac=' + list + '&mac_num=' + len + '&enable=1', function (data) {
        if (data.error == 0) {
            getMsg('操作成功！');
            if (len > 0) {
                if (len == 1) {
                    if (type == 1) {
                        $("#backList2 .liL").remove();
                    } else {
                        $("#backList .liL").remove();
                    }                    
                }
                if (mac == '') {
                    if(mob==0){
                        if (type == 1) {
                            $("#backList2").find('tr').eq(index).remove();
                        } else {
                            $("#backList").find('tr').eq(index).remove();
                        }
                    }else{
                        $("#backList").find('li').eq(index).remove();
                    }
                    
                } else {
                    var appendhtml = '<tr class="even"><td width="250">' + mac + '</td><td class="nobd"><a href="javascript:;" class="' + cls + '">取消黑名单</a></td></tr>';
                    var mobappendhtml='<li><div>'+mac+'</div><div><a href="javascript:;" class="mbpink">取消黑名单</a></div></li>';
                    if(mob==0){
                        if (type == 1) {
                            $("#backList2").append(appendhtml);
                        } else {
                            $("#backList").append(appendhtml);
                        }                       
                    }else{
                        $("#backList").append(mobappendhtml);
                    }

                    $("#black_mac").val('').siblings("label").show(); 
                
                }
            } else {
                if(mob==0){
                    if (type == 1) {
                        $("#backList2").empty().html(liHtml);
                    } else {
                        $("#backList").empty().html(liHtml);
                    }                    
                }else{
                    $("#backList").empty().html(mobliHtml);
                }

            }
        } else {
            locationUrl(data.error);
        }
    }, 'json');
}

function edit_black_list(mac, index, type, method) {
    mac=mac.toLowerCase();
    var tr = $("#backList").find('tr'),
    len = tr.length,
    macList = [],
    list = '',
    macLen = 0,
    maxNum = 32;

    if (type == 1) {
        tr = $("#backList2").find('tr');
        len = tr.length;
    }

    if (mob==1) {
        tr = $("#backList").find('li');
        len = tr.length;
    }

    if (method == 'del') {
        maxNum = 33;
    }

    if (len > 0 && len < maxNum) {
        var li = '';
        for (var i = 0; i < len; i++) {
            if (mob==0){
                li = tr.eq(i).children('td').eq(0).text();
            }else{
                li = tr.eq(i).children('div').eq(0).text();
            }
            
            if (index != i && li != '暂无内容' && li != '') {
                macList.push(li);
            }
        }

        var str = isRepeat(macList, mac);
        if (str == false) {
            getMsg('重复添加！');
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
    add_black_list(list, macLen, mac, index, type);
}




/*白名单*/
function white_list(type) {
    var html = '', len = 0, cls = 'wpink';
    if (type == 1) {
        cls = 'wpink2'
    }
    $.post(actionUrl + 'fname=net&opt=acl_white&function=get&ifindex=' + type, function (data) {
        if (data.error == 0) {
            len = data.mac.length;
            if (len > 0) {
                if (mob==0){
                    for (var i = 0; i < len; i++) {
                        html += '<tr class="even"><td width="250">' + data.mac[i] + '</td><td class="nobd"><a href="javascript:;" class="' + cls + '">取消白名单</a></td></tr>';
                    }
                }else{
                    for (var i = 0; i < len; i++) {
                        html += '<li ><div>' + data.mac[i] + '</div><div><a href="javascript:;" class="mwpink">取消白名单</a></div></li>';
                    }           
                }
            }
            if (html == '') {
                if(mob==0){
                    html = '<tr class="liL"><td colspan="3"><font size="+2">暂无内容</font></td></tr>';
                }else{
                    html = '<li class="liL"><h3>暂无内容</h3></li>';
                }
            }

            if (type == 0) {
                $("#whiteList").empty().html(html);
            } else {
                $("#whiteList2").empty().html(html);
            }

        } else {
            locationUrl(data.error);
        }
    }, 'json');
}
function add_white_list(list, len, mac, index, type) {
    var cls = 'wpink', liHtml = '<tr class="liL"><td colspan="3"><font size="+2">暂无内容</font></td></tr>',mobliHtml = '<li class="liL"><h3>暂无内容</h3></li>';
    if (type == 1) {
        cls = 'pink2';
    }
    $.ajaxSettings.async = false;
    $.post(actionUrl + 'fname=net&opt=acl_white&function=set&ifindex=' + type + '&mac=' + list + '&mac_num=' + len + '&enable=1', function (data) {
        if (data.error == 0) {
            getMsg('操作成功！');
            if (len > 0) {
                if (len == 1) {
                    if (type == 1) {
                        $("#whiteList2 .liL").remove();
                    } else {
                        $("#whiteList .liL").remove();
                    }
                }
                if (mac == '') {
                    if(mob==0){
                        if (type == 1) {
                            $("#whiteList2").find('tr').eq(index).remove();
                        } else {
                            $("#whiteList").find('tr').eq(index).remove();
                        }
                    }else{
                        $("#whiteList").find('li').eq(index).remove();
                    }
                } else {
                    var appendhtml = '<tr class="even"><td width="250">' + mac + '</td><td class="nobd"><a href="javascript:;" class="' + cls + '">取消白名单</a></td></tr>';
                    var mobappendhtml='<li><div>'+mac+'</div><div><a href="javascript:;" class="mwpink">取消白名单</a></div></li>';                    
                   
                    if(mob==0){
                        if (type == 1) {
                            $("#whiteList2").append(appendhtml);
                        } else {
                            $("#whiteList").append(appendhtml);
                        }                       
                    }else{
                        $("#whiteList").append(mobappendhtml);
                    }

                    $("#white_mac").val('').siblings("label").show(); 
                
                }

            } else {
                if(mob==0){
                    if (type == 1) {
                        $("#whiteList2").empty().html(liHtml);
                    } else {
                        $("#whiteList").empty().html(liHtml);
                    }                    
                }else{
                    $("#whiteList").empty().html(mobliHtml);
                }
            }
        } else {
            locationUrl(data.error);
        }
    }, 'json');
}

function edit_white_list(mac, index, type, method) {
    mac=mac.toLowerCase();
    var tr = $("#whiteList").find('tr'), len = tr.length, macList = [], list = '', macLen = 0, maxNum = 32;
    if (type == 1) {
        tr = $("#whiteList2").find('tr');
        len = tr.length;
    }
    if (mob==1) {
        tr = $("#whiteList").find('li');
        len = tr.length;
    }
    if (method == 'del') {
        maxNum = 33;
    }
    if (len > 0 && len < maxNum) {
        var li = '';
        for (var i = 0; i < len; i++) {
            if (mob==0){
                li = tr.eq(i).children('td').eq(0).text();
            }else{
                li = tr.eq(i).children('div').eq(0).text();
            }
            
            if (index != i && li != '暂无内容' && li != '') {
                macList.push(li);
            }
        }

        var str = isRepeat(macList, mac);
        if (str == false) {
            getMsg('重复添加！');
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
    add_white_list(list, macLen, mac, index, type);
}
/*白名单*/




/*黑白名单状态*/

var _switchMAC = {

    getblackSwitch:function(){ //获取黑名单状态
        $.post(actionUrl + 'fname=net&opt=acl_black&function=get&ifindex=0',function(data){
            if (data.error == 0) {
                if (data.enable==1){
                    if (mob==0){
                        $("#close_black_white").removeClass("selected");
                        $("#open_black_white").addClass("selected");
                    }else{
                        $("#wbswichBtn").removeClass("close-btn");
                    }
                    $("#wrap_whiteMAC_list").hide();
                    $("#wrap_blackMAC_list").show();
                }
            } else {
                locationUrl(data.error);
            }
        }, 'json');
    },
    getwhiteSwitch:function(){ //获取白名单状态
        $.post(actionUrl + 'fname=net&opt=acl_white&function=get&ifindex=0',function(data){
            if (data.error == 0) {
                if (data.enable==1){
                    if (mob==0){
                        $("#close_black_white").removeClass("selected");
                        $("#open_black_white").addClass("selected");
                    }else{
                        $("#wbswichBtn").removeClass("close-btn");
                    }
                    $("#wrap_blackMAC_list").hide();
                    $("#wrap_whiteMAC_list").show();
                }
            } else {
                locationUrl(data.error);
            }
        }, 'json');   
    }

}

/*黑白名单状态*/

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

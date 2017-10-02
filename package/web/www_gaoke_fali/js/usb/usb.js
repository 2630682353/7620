var space;
var pathNext;
var viewType;
var pageNumber;
var cpUrl;
var spaceData = '';
var navDataVideo = '';
var navDataMusic = '';
var navDataTxt = '';
var navDataImage = '';
var navDataPac = '';
var navNum;
var menuNum;
/*\
 * 获取usb存储信息
 */
function getUsbSpace() {
    $.getJSON(actionUrl + 'fname=sys&opt=storage&function=get&type=info', function (data) {
        layer.closeAll();
        if (data.error == 0) {
//            spaceData = data;
            spaceData = $.data(document.getElementById('usb_data'), data);
            spaceUsb(spaceData);
        } else {
            getMsg(getErrorCode(data.error));
        }
    });
}
/*
 * 获取usb存储文件夹信息
 */
function getUsbFile(url, start, num, startType) {
    var html = '';
    $.ajaxSettings.async = false;
    $.getJSON(actionUrl + 'fname=sys&opt=storage&function=get&type=dir&path=' + url + '&start=' + start + '&num=' + num, function (data) {
        if (data.error == 0) {
            var dir = data.dir, dir_len = dir.length, dirName = '';
            menuNum = dir_len;
            if (url.split('/').length > 3 && startType != 1) {
                html += '<li><a href="javascript:void(0);" path="' + url + '" class="parent_path" parent="1"><div class="file-icon folder-icon"><img class="thumb" style="visibility:hidden;"></div>';
                html += '<div class="file-name">上级目录...</div>';
                html += '<div class="file-size fl">&nbsp</div></a></li>';
            }
            if (dir_len > 0) {
                html = listView(html, url, dir, dir_len, dirName);
            }
            if ($("#usb_nav a").eq(0).hasClass('selected') === false) {
                $("#usb_nav a").removeClass('selected');
            }
            btnClick();
            $(".creat-btn").show();
            $(".upload-btn").show();
            if (startType == 1) {
                $("#list_view_box").append(html);
            } else {
                $("#list_view_box").empty().html(html);
            }
            if ($(".viewmode a").parent().hasClass('gridmode')) {
                $(".viewmode a").click();
            }
        } else {
            getMsg(getErrorCode(data.error));
        }
    });
}

function fileRm(type, url, status) {
    $.ajaxSettings.async = false;
    $.getJSON(actionUrl + 'fname=sys&opt=storage&function=set&type=act&act=' + type + '&' + url, function (data) {
        if (data.error == 0) {
            if (type == 'mkdir') {
                getMsg('新建操作成功！');
                getUsbFile($(".parent_path").attr('path'), 1, 20);
            } else {
                getInfo(type, status);
            }
        } else {
            getMsg(getErrorCode(data.error));
        }
    });
}

function getInfo(type, status) {
    loading('', $("#uploadLay_sch"), 1);
    var ProgressBar = {
        maxValue: 100,
        value: 0,
        SetValue: function (aValue) {
            this.value = aValue;
            if (this.value >= this.maxValue)
                this.value = this.maxValue;
            if (this.value <= 0)
                this.value = 0;
            var mWidth = this.value / this.maxValue * $("#progressBar_do_sch").width() + "px";
            $("#progressBar_Track_sch").css("width", mWidth);
            $("#progressBarTxt_sch").html(this.value + "%");
        }
    }
    ProgressBar.maxValue = 100;
    var index = 0;
    var mProgressTimer = window.setInterval(function () {
        $.ajaxSettings.async = false;
        $.getJSON(actionUrl + 'fname=sys&opt=storage&function=set&type=act&act=dump&info=' + type, function (data) {
            var msg = '';
            if (type == 'cp') {
                msg = '复制';
            } else if (type == 'rm') {
                msg = '删除';
            } else if (type == 'mv') {
                msg = '剪切';
                if (status == 2) {
                    msg = '重命名';
                }
            }
            if (data.error == 0) {
                index = data.sch;
                ProgressBar.SetValue(index);
                if (index == 100) {
                    window.clearInterval(mProgressTimer);
                    getMsg(msg + '操作完成！');
                    setTimeout(function () {
                        ProgressBar.SetValue(0);
                        layer.closeAll();
                    }, 1000);
                    var newListUrl = $(".parent_path").attr('path');
                    var nav = $("#usb_nav a"), cl = nav.hasClass('selected'), inde = 0;
                    nav.each(function (index) {
                        if ($(this).hasClass('selected')) {
                            inde = index;
                        }
                    });
                    var str = nav.eq(inde).text();
                    if (inde != 0) {
                        type = usbNavType(str);
                        usbNav(type, 1, 20);
                    } else {
                        getUsbFile(newListUrl, 1, 20);
                    }
                    btnClick();
                    if (type == 'mv') {
                        $.cookie('url', null);
                        $.cookie('type', null);
                    }
                }
            } else {
                window.clearInterval(mProgressTimer);
                getMsg(msg + '操作失败,错误码：' + getErrorCode(data.error));
                setTimeout(function () {
                    layer.closeAll();
                }, 3000);
            }
        });
    }, 1000);
}

function usbNav(type, start, num, navType) {
    $.ajaxSettings.async = false;
    $.post(actionUrl + 'fname=sys&opt=storage&function=set&type=file&if=' + type + '&path=' + space.path + '&start=' + start + '&num=' + num, function (data) {
        layer.closeAll();
        if (data.error == 0) {
            if (navType != 1) {
                navTypeData(data, type);
            }
            navHtml(data, navType);
        } else {
            getMsg(getErrorCode(data.error));
        }
    }, 'json');
}

function bytesToSize(bytes) {
    if (bytes === 0)
        return '0 B';
    var k = 1024,
            sizes = ['B', 'KB', 'MB', 'GB', 'TB', 'PB', 'EB', 'ZB', 'YB'],
            i = Math.floor(Math.log(bytes) / Math.log(k));
    return (bytes / Math.pow(k, i)).toPrecision(3) + ' ' + sizes[i];
}

function width(type) {
    var width;
    switch (type) {
        case 'video':
            width = (space.video[1] / space.total * 100).toFixed(6) + '%';
            $("#tank_video").attr('style', 'width:' + width).attr('title', width);
            break;
        case 'pic':
            width = (space.pic[1] / space.total * 100).toFixed(6) + '%';
            $("#tank_image").attr('style', 'width:' + width).attr('title', width);
            break;
        case 'txt':
            width = (space.txt[1] / space.total * 100).toFixed(6) + '%';
            $("#tank_filebag").attr('style', 'width:' + width).attr('title', width);
            break;
        case 'music':
            width = (space.music[1] / space.total * 100).toFixed(6) + '%';
            $("#tank_music").attr('style', 'width:' + width).attr('title', width);
            break;
        case 'pack':
            width = (space.pack[1] / space.total * 100).toFixed(6) + '%';
            $("#tank_bag").attr('style', 'width:' + width).attr('title', width);
            break;
        case 'other':
            width = (space.other[1] / space.total * 100).toFixed(6) + '%';
            $("#tank_other").attr('style', 'width:' + width).attr('title', width);
            break;
    }
}

function getLocalTime(time) {
    return new Date(parseInt(time) * 1000).toLocaleString().replace(/:\d{1,2}$/, ' ');
}

function checkClass(name, status) {
    var type = '', typeClass = '';
    if (status != 1) {
        type = name.split('.').pop();
    } else {
        type = name;
    }
    typeClass = 'list-file-icon list-' + type + '-icon';
    return typeClass;
}

function addPage(page) {
    var html = '<a href="javascript:;" data-bind="' + page + '" class="next-page">' + page + '</a>';
    return html;
}

function getFileInfo(path) {
    var info = {};
    info.name = path.split('/').pop();
    info.type = path.split('.').pop();

    return info;
}

function listView(html, url, dir, dir_len, dirName) {
    var type = '';
    for (var i = 0; i < dir_len; i++) {
        html += '<li>';
        dirName = url + '/' + dir[i].name;
        if (dir[i].type == 'D') {
            html += '<span class="chksp"></span><a href="javascript:void(0);" type="D" path="' + dirName + '" title="' + dir[i].name + '"><div class="file-icon folder-icon"><img class="thumb" style="visibility:hidden;"></div>';
            html += '<div class="file-name">' + dir[i].name + '</div>';
            html += '<div class="file-size fl">&nbsp</div></a>';
        } else if (dir[i].type == 'F') {
            type = dir[i].name.split('.').pop();
            html += '<span class="chksp"></span><a href="' + dirName + '" target="_blank" title=' + dir[i].name + '><div class="file-icon ' + type + '-icon"><img class="thumb" style="visibility:hidden;"></div>';
            html += '<div class="file-name fl">' + dir[i].name + '</div></a>';
            html += '<div class="file-size fl">' + bytesToSize(dir[i].size) + '</div>';
        }
        html += '<div class="file-time fl">' + getLocalTime(dir[i].time) + '</div>';
        html += '</li>';
    }
    return html;
}

function getTree(_this, url, width) {
    var html = '';
    $.ajaxSettings.async = false;
    $.getJSON(actionUrl + 'fname=sys&opt=storage&function=get&type=dir&path=' + url + '&start=1&num=100', function (data) {
        if (data.error == 0) {
            var dir = data.dir, dir_len = dir.length, dirName = '';
            width = Number(width) + 15;
            if (dir_len > 0) {
                for (var i = 0; i < dir_len; i++) {
                    if (dir[i].type == 'D') {
                        html += '<li><div class="treeview-node" data-width="' + width + '" style="padding-left:' + width + 'px;">';
                        dirName = url + '/' + dir[i].name;
                        html += '<span class="treeview-node-handler"><em class="tree_icon"></em><em class="folde_icon"></em>';
                        html += '<span data-url="' + dirName + '">' + dir[i].name + '</span></span></div><ul class="treeview treeview-collapse"></ul></li>';
                    }
                }
            }
            cpUrl = _this.find('span').find('span').attr('data-url');
            _this.next().append(html);
        } else {
            getMsg(getErrorCode(data.error));
        }
    });
}

function spaceUsb(data) {
    var len = data.info.length;
    if (len == 0) {
        $(".no-usb").show();
        $("#usbmem").hide();
        $("#usb_file").hide();
    } else {
        $(".no-usb").hide();
        $("#m_12_box").find('.bd-box').show();
        space = data.info[0];
        $("#usedspace").text(bytesToSize(space.used));
        $("#totalspace").text(bytesToSize(space.total));
        for (x in space) {
            width(x);
        }
        getUsbFile(space.path, 1, 20);
    }
}

function navHtml(data, type) {
    var dir = data.file, dir_len = dir.length, dirName = '', html = '', FileInfo = '', fileType = '', path = space.path;
    navNum = dir_len;
    if (dir_len > 0) {
        for (var i = 0; i < dir_len; i++) {
            html += '<li>';
            FileInfo = getFileInfo(dir[i].path);
            dirName = FileInfo.name;
            fileType = FileInfo.type;
            html += '<span class="chksp"></span><a href="' + path + dir[i].path + '" target="_blank" title="' + dir[i].path + '"><div class="file-icon ' + fileType + '-icon"><img class="thumb" style="visibility:hidden;"></div>';
            html += '<div class="file-name fl">' + dirName + '</div>';
            html += '<div class="file-size fl">' + bytesToSize(dir[i].size) + '</div></a>';
            html += '</li>';
        }
    }
    $(".creat-btn").hide();
    $(".upload-btn").hide();
    $("#checkAll").removeClass('chksp_on');
    if (type == 1) {
        $("#list_view_box").append(html);
    } else {
        $("#list_view_box").empty().html(html);
    }
    if ($(".viewmode a").parent().hasClass('gridmode')) {
        $(".viewmode a").click();
    }
}

function navTypeData(data, type) {
    switch (type) {
        case 'video':
            navDataVideo = data;
            break;
        case 'music':
            navDataMusic = data;
            break;
        case 'pic':
            navDataImage = data;
            break;
        case 'txt':
            navDataTxt = data;
            break;
        case 'pack':
            navDataPac = data;
            break;
    }
}

function navType(type) {
    var data;
    switch (type) {
        case 'video':
            data = navDataVideo;
            break;
        case 'music':
            data = navDataMusic;
            break;
        case 'pic':
            data = navDataImage;
            break;
        case 'txt':
            data = navDataTxt;
            break;
        case 'pack':
            data = navDataPac;
            break;
    }
    return data;
}

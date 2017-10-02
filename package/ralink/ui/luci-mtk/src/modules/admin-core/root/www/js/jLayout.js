function jLayouts($, jLayoptns) {
	// need jQuery 1.4 ++
	var jLay = this;
	jLay.monitor=function(){return{
			prm:{},evs:{},add:function(ev){for(x in ev){this.evs[x]=ev[x]}},bev:function(name,_fn){this.evs[name]=_fn;},
			run:function(name){function runev(_evs){if(typeof(_evs)=='function'){_evs.apply(this, [].slice.call(arguments, 1))}if(typeof(_evs)=='object'){$.each(_evs,function(id,it){runev(it)});}};if(name){runev(this.evs[name])}else{runev(this.evs);}}
	}}
	jLay.options = {bodyminwidth: 600,body: true,wheelStepSize: 120,none: null}
	jLay.mode={'x':{axs:'x',pos :'left',siz:'width',sz:'Width'},'y':{axs:'y',pos :'top',siz:'height',sz:'Heigth'}};
	jLay.stepper=function(){return{
			step:null,steps:300,speed:5,interval:10,tmtid:null,
			onBof:function(){},onStp:function(){},onEof:function(){},onRun:function(){},
			run:function(){
				var ths=this;
				var runsteppr=function(t){
					ths.onRun(t);
					if(t==0){ths.onBof(t);}
					if(ths.step && (t!=0)&&(t%(ths.step/ths.speed)==0)){ths.onStp(t);}
					if(t>=(ths.steps/ths.speed)){clearTimeout(ths.tmtid);ths.onEof(t);}else{
						ths.tmtid=setTimeout(function(){runsteppr(t+1)},ths.interval);
					}
				}
				runsteppr(0);
		       }
	}};
    if (jLayoptns) { $.extend(jLay.options, jLayoptns); }
    jLay.scrollbarsize = gScrollbarSize();

    jLay.button = function (optns) {
        var button = this;
        var options = { trg: '.btn', cls: null };
        $.extend(options, optns);
        $(document).delegate(options.trg,'mouseover', function (event) { $(this).addClass(gClass(this, options.cls) + '_ovr') });
        $(document).delegate(options.trg,'mouseout',function (event) { if (isout(event, this)) { $(this).removeClass(gClass(this, options.cls) + '_ovr') } });
        $(document).delegate(options.trg,'mousedown',function () { $(this).addClass(gClass(this, options.cls) + '_prs') });
        $(document).delegate(options.trg,'mouseup',function(){ $(this).removeClass(gClass(this, options.cls) + '_prs') });
        $(document).bind('mouseup',function(){ $(options.trg).removeClass(function () { return gClass(this, options.cls) + '_prs'; }) });
        return button;
    }//==================================================================================
    jLay.tab = function (optns) {
        var tab = this;
        var options = {
            trgarea: '.tbs',
            trg: '.tb',
            rsparea: '.tbws',
            rsp: '.tbw',
            index: 0,
            lev: 3,
            ev: 'click'
        };
        $.extend(options, optns);

        var trgareas = $(options.trgarea);
        trgareas.each(function (i) {
            var rsparea,trgarea = $(this),ev=getEvn(trgarea,options.ev);

            if (trgarea.attr('rsp')) {
                rsparea = gNear(trgarea, trgarea.attr('rsp'), options.lev);
            } else {
                rsparea = gNear(trgarea, options.rsparea, options.lev);
            }

            if (!rsparea.eq(0)[0]) {return;}

            var trgs = trgarea.find(options.trg);
            var rsps = rsparea.find(options.rsp);

            if (trgs[options.index] && rsps[options.index]) {
            	onoff([$(trgs[options.index]),$(rsps[options.index])]);
            }
            
            switch (ev) {
                case 'click':
                    trgs.click(function () {
                        var cls = gClass(this);
                        var idx = trgs.index(this);
                        if (!rsps[idx]) { return; }
                        
                        trgs.removeClass(cls + '_on');
                        rsps.removeClass(function () { return gClass(this) + '_on'; });
                   
                        onoff($(rsps[idx]));
                        onoff($(this));
                    });
                    break;
                case 'hover':
                    trgs.mouseover(function () {
                        var cls = gClass(this);
                        var idx = trgs.index(this);

                        if (!rsps[idx]) { return; }

                        trgs.removeClass(cls + '_on');
                        rsps.removeClass(function () { return gClass(this) + '_on'; });
                        
                        onoff($(rsps[idx]));
                        onoff($(this));
                    });
                    break;
            }
        });
        return tab;
    }//==================================================================================
	jLay.status=function(optns){return new jLaystatus(optns);}
    function jLaystatus(optns){
       var stt=this;
	var options={
		trg:null,
		rsp:null,
		ev:'click',
		blur:true,
		lev:1
	};
	$.extend(options, optns);
	
	stt.trgs=$(document).find(options.trg);
	stt.cTrg,stt.cRsp;
	var ev=options.ev;
	var globalClick=function(event){
		if(!stt.cTrg){return}
		stt.trgs.each(function(){var it=$(this);if( (it[0]!=stt.cTrg) && (getEvn(it,ev)=='click') ){var r=getRsp(it,options.rsp,options.ev);onoff([it,r],'off');}});
		if(stt.cRsp){
			if(isout(event,stt.cTrg) && isout(event,stt.cRsp)){onoff([stt.cTrg,stt.cRsp],'off');}
		}else{
			if(isout(event,stt.cTrg)){onoff(stt.cTrg,'off');}
		}
	}
	if(options.blur){$(document).bind('click',function(event){globalClick(event);});}
	$(document).delegate(options.trg,'click',
		function(event){
			if(getEvn(this,ev)!='click'){return;}
			var trg=$(this),rsp=getRsp(trg,options.rsp,options.lev);
			stt.cTrg=trg[0];
			onoff(trg,'toggle');
			if(rsp[0]){
				stt.cRsp=rsp[0];
				onoff(rsp,'toggle');
			}
		}
	);
	$(document).delegate( options.trg,'mouseover',
		function(event){
			if(getEvn(this,ev)!='hover'){return;}
			var trg=$(this),rsp=getRsp(trg,options.rsp,options.lev);
			onoff(trg);
			if(rsp.eq(0)[0]){
				onoff(rsp);
				if(!trg.data('fixed')){
					rsp.bind('mouseout',function(event){if(isout(event,trg) && isout(event,rsp)){onoff([trg,rsp],'off');}});
					trg.data('fixed',true);
				}
			}
		}
	);
	$(document).delegate( options.trg,'mouseout',
		function(event){
			if(getEvn(this,ev)!='hover'){return;}
			var trg=this,rsp=getRsp(trg,options.rsp,options.lev);
			if(rsp.eq(0)[0]){
				if( isout(event,trg) && isout(event,rsp)){ onoff([trg,rsp],'off'); }
			}else{
				if(isout(event,trg)){onoff(trg,'off');}
			}
			
			
		}
	);
	return stt ;
    }//==================================================================================
	jLay.size=function(optns){return new jLaysize(optns);}
    function jLaysize(optns){
		var size=this;
		var options={
			trg:null,
			mode:'100%',//'screen','element'
			rsp:null,
			dir:'x',
			max:null,min:null,
			ett:0,
			area:'body',
			onsize:function(){},
			onmax:function(){},
			onmin:function(){}
		};
		$.extend(options, optns);
		
		var trgs=$(document).find(options.trg);
		if(trgs.length<1){return;}
		options.ett=parseInt(options.ett);
		
		var md=jLay.mode[options.dir];
		function siz(o){
			var sizn=0,area;o=$(o);
			if(!o.eq(0)[0]){return;}
			switch(options.mode){
				case '100%' :
					o.css(md.siz,'100%');
					sizn=o[md.siz]();
					if(options.max){if( sizn > options.max){sizn=options.max;}}
					if(options.min){if( sizn < options.min){sizn=options.min;}}
				    	o.css(md.siz,sizn-options.ett);
			    	break;
			    	case 'screen' :
			    		sizn=$(window)[md.siz]();
			    		if(options.max){if( sizn > options.max){sizn=options.max}}
					if(options.min){if( sizn < options.min){sizn=options.min}}
					var _css={};_css[md.siz]=sizn-options.ett;
				    	o.css(md.siz,sizn-options.ett);
			    	break;
			    	case 'element' :
			    		var rsp=$(document).find(options.rsp);
			    		if(!rsp.eq(0)[0]){return;};
			    		o.css(md.siz,(rsp[md.siz]())-options.ett);
			    	break;
			    	case 'parent':
			    		var rsp=gparent(o,options.rsp,7);
			    		if(!rsp[0]){return;};
			    		o.css(md.siz,(rsp[md.siz]())-options.ett);
			    	break;
		    	}
		    	
		    	options.onsize(sizn,$(window)[md.siz]());
		    	if(options.max && ( sizn == options.max) ){options.onmax()}
				if(options.min && ( sizn == options.min) ){options.onmin()}
		    	
		}
		sizeall();
		var timer,locked=false;
		if(options.mode!=='parent'){
			window.onresize=function(){
			  clearTimeout(timer);
			  if(!locked){locked=true;sizeall();}
			  timer = setTimeout(function(){locked=false},1);
			};
		}
		function sizeall(){trgs.each(function(){siz($(this));});}
		return size;
	}//===================================================================================================================

    jLay.list = function (optns) {
        var list = this;
        var options = {
            trg: '.list',
            li: '.li',
            high: true,
            col: 1
        };
        $.extend(options, optns);
        var trgs = $(options.trg);
        trgs.each(function () {
            var trg = $(this);
            trg.addClass('dl dl_' + gClass(trg));
            var lis = trg.find(options.li);
            if (lis.length < 1) { return false }
            var lev = Math.ceil(lis.length / options.li);
            var tag = lis.eq(0).get(0).tagName.toLowerCase();
            var cls = gClass(lis.eq(0));
            var n = 1, c = 1, lic = [];
            lis.each(function (idx, itm) {
                lic[lic.length] = [itm, parseInt($(itm).height())];
                var colcls = cls + 'c_' + c;
                if (options.col == 0) { colcls = "" }
                if (options.high) { $(itm).addClass(cls + '_' + n + ' ' + colcls) }
                if (((n % options.col == 0) || (n == (lis.length))) && (idx != 0) && options.col != 0) {
                    $(itm).after('<' + tag + ' class="' + cls + '_br"></' + tag + '>');
                    if (options.high) {
                        lic.sort(function (a, b) { if (a[1] > b[1]) { return -1; } else { return 1; } });
                        for (i = 0; i < lic.length; i++) { $(lic[i][0]).css('height', lic[0][1]); }
                        lic = [];
                    }
                    c = 0;
                }
                c++;
                n++;
            });
        });
        return list;
    }//==================================================================================
    jLay.marquee = function (optns) {
        var marquee = this;
        var options = {
            trg: '.marquee',
            interval: 500,
            distance: 5,
            direction: 'y',
            cycling: true
        };
        $.extend(options, optns);
        var trgs = $(options.trg);
        var dir = options.direction, siz, pos;
        if (dir == 'x') { siz = 'width'; pos = 'left'; } else if (dir == 'y') { siz = 'height'; pos = 'top'; } else { return }

        trgs.each(function () {
            var trg = $(this), trgItvid;
            if (gppty(trg, 'scroll' + siz) <= gppty(trg, siz)) { return }
            var mrq = function () {
                var scrollto = gppty(trg);
                if (options.cycling) {
                    if ((gppty(trg, 'scroll' + pos) + gppty(trg, siz)) >= gppty(trg, 'scroll' + siz)) {
                        jLay.scrollto(dir, trg, 0);
                    }
                }
                jLay.scrollto(dir, trg, gppty(trg, 'scroll' + pos) + options.distance);
            }
            trgItvid = setInterval(mrq, options.interval);
            trg.mouseover(function () { clearInterval(trgItvid); });
            trg.mouseout(function (event) {
                if (isout(event, this)) {
                    trgItvid = setInterval(mrq, options.interval);
                }
            });
            trg.mousewheel(function (event, delta) {
                var scrollSize = gppty(trg, 'scroll' + pos);
                if (delta < 0) { scrollSize += jLay.options.wheelStepSize; }
                if (delta > 0) { scrollSize -= jLay.options.wheelStepSize; }
                jLay.scrollto(dir, this, scrollSize);
                return false;
            });
        });

    }
    jLay.scrollto = function (dir, o, to) { if (dir == 'x') { $(o).scrollLeft(to) }; if (dir == 'y') { $(o).scrollTop(to) } }
    
    //=====================================================================
    	function getEvn(trg,ev){trg=$(trg);var evn=ev?ev:null;evns=['hover','click'];if(inArr(evns,trg.attr('ev'))){evn=trg.attr('ev')}return evn;}
	function getRsp(trg,fnd,lev){trg=$(trg);var rsp=null;if(trg.attr('rsp')){rsp=gNear(trg,trg.attr('rsp'),lev)}else{rsp=gNear(trg,fnd,lev);}return rsp;}
	function inArr(arr,key){if(arr.length<1 || !key ){return false;};for(var x=0;x<arr.length;x++){if(arr[x]==key){return true}}return false;}
	function gparent(_o, _slt, _lev){ 
		if(!_slt){return $();}
		if($(_o).parent(_slt)[0]){ return $(_o).parent(_slt); } 
		if(!_lev){_lev = 10}; 
		var prts=$(_o).parent(),prtslt=$(),c = 0; 
		while (!prtslt[0] && (c < _lev)) {
			 	prtslt = prts.parent(_slt); prts = prts.parent(); c++; 
		};
		return prtslt;
		}
	function runOnce(f){var run; return function() {if (!run) {run = true; f.apply(this, arguments);} }}
	function gNear(e, find, lev) { if (e.find(find).eq(0)[0]) { return e.find(find).eq(0); }; var eParent = e.parent(), o = null, c = 0; while (!o && ((eParent != document.body) && (c < lev))) { o = eParent.find(find).eq(0)[0]; eParent = eParent.parent(); c++; }; return $(o); }
	function gClass(e, clss) { if (clss) { $(e).addClass(clss); return clss; }; var cln = 'jl'; if (!$(e).attr('class')) { $(e).addClass(cln); } if ($(e).attr('class')) { classn = $(e).attr('class').split(' '); if (classn.length > 0) { cln = classn[0]; } } return cln; }
	function isout(event,e){var tgt=event.target;if(event.type=="mouseout"){tgt=event.relatedTarget}if (tgt == $(e)[0] || $(e).find(tgt)[0]) { return false; } else { return true }; };
	function onoff(trg,turn){
		if(!turn){turn='on'};
		switch(turn){
			case 'on':
				if($.type(trg)=='array'){
					for(var x=0;x<trg.length;x++){
						$(trg[x]).addClass(function(){return gClass(this)+'_on'});
					}
				}else{
					$(trg).addClass(gClass(trg)+'_on');
				}break;
			case 'off':
				if($.type(trg)=='array'){
					for(var x=0;x<trg.length;x++){
						$(trg[x]).removeClass(function(){return gClass(this)+'_on'});
					}
				}else{
					$(trg).removeClass(gClass(trg)+'_on');
				}break;
			case 'toggle':
				if( trg.length > 0 ){
					var status='on';
					if($(trg[0]).hasClass(gClass(trg[0])+'_on')){status="off"}
					
					for(var x=0;x<trg.length;x++){
						if(status=='on'){
							$(trg[x]).addClass(function(){return gClass(this)+'_on'});
						}else{
							$(trg[x]).removeClass(function(){return gClass(this)+'_on'});
						}
					}
				}else{
					if($(trg).hasClass(gClass(trg)+'_on')){
						$(trg).removeClass(gClass(trg)+'_on');
					}else{
						$(trg).addClass(gClass(trg)+'_on');
					}
				}
				break;
		}
	}

    function onStartDrag(event) { if (event.preventDefault) { event.preventDefault(); } else { event.returnValue = false; }; if ($.browser.msie) { $('html').bind('dragstart', stoped).bind('selectstart', stoped); }; return false; };
    function onStopDrag() { if ($.browser.msie) { $('html').unbind('dragstart', stoped).unbind('selectstart', stoped); } };
    function stoped(){return false;}
    function epage(event) { var page = { 'x': event.pageX, 'y': event.pageY }; return page; }
    function gppty(o, ppt) {
        var property = {
            'top': function () { return parseInt(o.css('top')); }, 'left': function () { return parseInt(o.css('left')) },
            'width': function () { return parseInt(o.width()) }, 'height': function () { return parseInt(o.height()) },
            'offsettop': function () { return parseInt(o.offset().top) }, 'ofsetleft': function () { return parseInt(o.offset().left) },
            'scrollwidth': function () { return parseInt(o.get(0).scrollWidth) }, 'scrollheight': function () { return parseInt(o.get(0).scrollHeight) },
            'clientwidth': function () { return parseInt(o.get(0).clientWidth) }, 'clientheight': function () { return parseInt(o.get(0).clientHeight) },
            'offsetwidth': function () { return parseInt(o.get(0).offsetWidth) }, 'offsetheight': function () { return parseInt(o.get(0).offsetHeight) },
            'scrolltop': function () { return parseInt(o.scrollTop()); }, 'scrollleft': function () { return parseInt(o.scrollLeft()); },
            'scrollto': function (d, p) { if (d == 'x') { o.scrollLeft(p) }; if (d == 'y') { o.scrollTop(p) } }
        }; if (ppt) { return property[ppt](); } else { return property; }
    }
    function rKyvl(_obj) { var _key, _val; for (x in _obj) { _key = x; _val = _obj[x]; }; return { key: _key, val: _val }; };
    function gScrollbarSize() { $('body').append('<div id="scrollSizeTest" style="position:absolute;visibility:hidden;width:100px;height:100px;overflow:scroll;"></div>'); var r = { 'x': parseInt($('#scrollSizeTest').get(0).offsetWidth) - parseInt($('#scrollSizeTest').get(0).clientWidth), 'y': parseInt($('#scrollSizeTest').get(0).offsetHeight) - parseInt($('#scrollSizeTest').get(0).clientHeight) }; $('#scrollSizeTest').remove(); return r; }
    return jLay;
};
;(function (a) { function d(b) { var c = b || window.event, d = [].slice.call(arguments, 1), e = 0, f = !0, g = 0, h = 0; return b = a.event.fix(c), b.type = "mousewheel", c.wheelDelta && (e = c.wheelDelta / 120), c.detail && (e = -c.detail / 3), h = e, c.axis !== undefined && c.axis === c.HORIZONTAL_AXIS && (h = 0, g = -1 * e), c.wheelDeltaY !== undefined && (h = c.wheelDeltaY / 120), c.wheelDeltaX !== undefined && (g = -1 * c.wheelDeltaX / 120), d.unshift(b, e, g, h), (a.event.dispatch || a.event.handle).apply(this, d) } var b = ["DOMMouseScroll", "mousewheel"]; if (a.event.fixHooks) for (var c = b.length; c;) a.event.fixHooks[b[--c]] = a.event.mouseHooks; a.event.special.mousewheel = { setup: function () { if (this.addEventListener) for (var a = b.length; a;) this.addEventListener(b[--a], d, !1); else this.onmousewheel = d }, teardown: function () { if (this.removeEventListener) for (var a = b.length; a;) this.removeEventListener(b[--a], d, !1); else this.onmousewheel = null } }, a.fn.extend({ mousewheel: function (a) { return a ? this.bind("mousewheel", a) : this.trigger("mousewheel") }, unmousewheel: function (a) { return this.unbind("mousewheel", a) } }) })(jQuery);

// 0xjah.me - Main JS
const $=s=>document.getElementById(s),$$=s=>document.querySelectorAll(s);
const showBtn=v=>{const b=$("end-button-container");if(b)b.style.display=v?"block":"none"};
const scroll=d=>setTimeout(()=>(document.querySelector("h1")||window).scrollIntoView?.({behavior:"smooth",block:"start"})||window.scrollTo({top:0,behavior:"smooth"}),d||100);

// Theme Toggle
const getTheme=()=>localStorage.getItem("theme")||(window.matchMedia("(prefers-color-scheme:light)").matches?"light":"dark");
const setTheme=t=>{document.documentElement.setAttribute("data-theme",t);localStorage.setItem("theme",t)};
window.toggleTheme=()=>setTheme(getTheme()==="dark"?"light":"dark");
document.addEventListener("DOMContentLoaded",()=>{setTheme(getTheme())});


// Gallery
window.openModal=u=>{const m=$("imageModal"),i=$("modalImage");if(m&&i){i.src=u;m.classList.add("active");document.body.style.overflow="hidden"}};
window.closeModal=()=>{const m=$("imageModal");if(m){m.classList.remove("active");document.body.style.overflow="auto"}};
document.addEventListener("keydown",e=>e.key==="Escape"&&closeModal());

// Blog
window.filterCategory=c=>{$$("#blog-posts li").forEach(p=>p.style.display=c==="all"||p.dataset.category===c?"":"none");$$(".category-btn").forEach(b=>b.classList.remove("active"));document.querySelector(`.category-btn[onclick*="${c}"]`)?.classList.add("active")};

// HTMX
document.addEventListener("htmx:beforeRequest",e=>{if(e.detail.requestConfig.verb==="get"){const c=location.pathname+location.search,t=e.detail.requestConfig.path;if(c===t||(location.pathname==="/"&&t==="/"))e.preventDefault()}});
document.addEventListener("htmx:responseError",e=>{const t=e.target,m={"status-header":"Status unavailable","projects-list":"Projects unavailable",email:"Email unavailable"}[t.id];if(m)t.innerHTML=`<div class="error">${m}</div>`});
document.addEventListener("htmx:timeout",e=>e.target.innerHTML='<div class="error">Request timed out</div>');

// Blog init
document.addEventListener("DOMContentLoaded",()=>{
  if(!/^\/blog(\.html)?$/.test(location.pathname))return;
  const content=$("blog-post-content");
  const hasContent=()=>content&&content.innerHTML.trim().length>0;
  
  // Show button if content exists (server-rendered)
  showBtn(hasContent());
  
  // Only fetch via HTMX if content is empty (not server-rendered)
  const p=new URLSearchParams(location.search).get("post");
  if(p&&!hasContent()&&typeof htmx!=="undefined"){
    htmx.ajax("GET",`/blog?post=${p}`,{target:"#blog-post-content",swap:"innerHTML"}).then(()=>{showBtn(1);scroll(300)});
  }
});

// Global HTMX listeners for blog (must be outside DOMContentLoaded)
document.addEventListener("htmx:afterSwap",e=>{
  if(e.detail.target.id==="blog-post-content"){showBtn(1);scroll()}
  if(e.detail.target.classList?.contains("site-container")){showBtn(0);const c=$("blog-post-content");if(c)c.innerHTML="";scroll()}
});
document.addEventListener("htmx:beforeTransition",e=>{
  if(!e.detail.path?.includes("?post=")){showBtn(0);const c=$("blog-post-content");if(c)c.innerHTML=""}
});

// 0xjah.xyz - Main JS
const $=s=>document.getElementById(s),$$=s=>document.querySelectorAll(s);

// Theme Toggle
const getTheme=()=>localStorage.getItem("theme")||(window.matchMedia("(prefers-color-scheme:light)").matches?"light":"dark");
const setTheme=t=>{document.documentElement.setAttribute("data-theme",t);localStorage.setItem("theme",t)};
window.toggleTheme=()=>setTheme(getTheme()==="dark"?"light":"dark");
document.addEventListener("DOMContentLoaded",()=>{
  setTheme(getTheme());
  document.querySelectorAll(".resume-link[data-resume-url]").forEach(a=>{
    a.addEventListener("click",e=>{
      e.preventDefault();
      const url=a.getAttribute("data-resume-url");
      if(!url)return;
      const name=a.getAttribute("data-resume-name")||"";
      const link=document.createElement("a");
      link.href=url;
      if(name)link.download=name;
      link.rel="noopener";
      document.body.appendChild(link);
      link.click();
      link.remove();
    });
  });
});


// Gallery
window.openModal=(u,alt)=>{const m=$("imageModal"),i=$("modalImage");if(m&&i){i.src=u;i.alt=alt||"";m.classList.add("active");document.body.style.overflow="hidden"}};
window.closeModal=()=>{const m=$("imageModal");if(m){m.classList.remove("active");document.body.style.overflow="auto"}};
document.addEventListener("keydown",e=>e.key==="Escape"&&closeModal());

// Blog
window.filterCategory=c=>{$$("#blog-posts li").forEach(p=>p.style.display=c==="all"||p.dataset.category===c?"":"none");$$(".category-btn").forEach(b=>b.classList.remove("active"));document.querySelector(`.category-btn[onclick*="${c}"]`)?.classList.add("active")};

// HTMX
document.addEventListener("htmx:beforeRequest",e=>{if(e.detail.requestConfig.verb==="get"){const c=location.pathname+location.search,t=e.detail.requestConfig.path;if(c===t||(location.pathname==="/"&&t==="/"))e.preventDefault()}});
document.addEventListener("htmx:responseError",e=>{const t=e.target,m={"status-header":"Status unavailable","projects-list":"Projects unavailable",email:"Email unavailable"}[t.id];if(m)t.innerHTML=`<div class="error">${m}</div>`});
document.addEventListener("htmx:timeout",e=>e.target.innerHTML='<div class="error">Request timed out</div>');

// Blog terminal: posts open in a <dialog> over the list.
// Listeners are on document/window so they survive htmx boost swapping <body>.
const postCache=new Map();
const slugOf=a=>new URL(a.href,location.href).searchParams.get("post");
const fetchPost=slug=>{
  if(!postCache.has(slug)){
    const req=fetch(`/blog?post=${encodeURIComponent(slug)}`,{headers:{"HX-Request":"true"}})
      .then(r=>{if(!r.ok)throw new Error(r.status);return r.text()});
    req.catch(()=>postCache.delete(slug)); // don't cache failures
    postCache.set(slug,req);
  }
  return postCache.get(slug);
};
let openSlug=null,blogTitle="Blog | 0xjah.xyz",downOnBackdrop=false,ownBack=false;

const openPost=async(slug,push)=>{
  const term=$("post-term");
  if(!term||!slug)return;
  openSlug=slug;
  const root=document.documentElement;
  root.style.cursor="progress";
  let html;
  try{html=await fetchPost(slug)}
  catch{html='<div class="error">Couldn\'t load this post. Try again.</div>'}
  finally{root.style.cursor=""}
  if(openSlug!==slug)return; // a newer post was requested meanwhile
  if(!term.open)blogTitle=document.title;
  $("blog-post-content").innerHTML=html;
  $("term-file").textContent=`${slug}.md`;
  $("term-body").scrollTop=0;
  const h=term.querySelector(".blog-post h1");
  document.title=`${h?h.textContent.trim():slug} | 0xjah.xyz`;
  if(push)history.pushState({post:slug},"",`/blog?post=${encodeURIComponent(slug)}`);
  if(!term.open)term.showModal();
  $("term-body").focus({preventScroll:true});
};
const closePost=()=>{const t=$("post-term");if(t?.open)t.close()};

// "close" doesn't bubble, so listen in the capture phase.
document.addEventListener("close",e=>{
  if(e.target.id!=="post-term")return;
  openSlug=null;
  $("blog-post-content").innerHTML="";
  document.title=blogTitle;
  if(new URLSearchParams(location.search).has("post")){
    if(history.state?.post){ownBack=true;history.back()} // opened in-page: return to the list entry
    else history.replaceState(null,"","/blog");  // landed directly on the post
  }
},true);

// Capture phase so this runs before htmx's boost handlers on the links.
document.addEventListener("click",e=>{
  const term=$("post-term");
  if(!term)return;
  if(e.target===term){if(downOnBackdrop)closePost();return} // click on the backdrop
  const a=e.target.closest?.("a");
  if(!a||e.button||e.metaKey||e.ctrlKey||e.shiftKey||e.altKey)return; // let new-tab clicks through
  if(a.matches("#blog-posts a"))openPost(slugOf(a),true);
  else if(a.matches("[data-term-close]"))closePost();
  else if(a.matches("[data-term-top]"))$("term-body").scrollTo({top:0,behavior:"smooth"});
  else return;
  e.preventDefault();
  e.stopPropagation();
},true);
// Only close on backdrop clicks that also started there (not text selections dragged outside).
document.addEventListener("pointerdown",e=>{downOnBackdrop=e.target.id==="post-term"},true);

// Prefetch on hover/focus/touch so opening feels instant.
const prefetch=e=>{const a=e.target.closest?.("#blog-posts a");if(a)fetchPost(slugOf(a)).catch(()=>{})};
document.addEventListener("pointerover",prefetch,{passive:true});
document.addEventListener("focusin",prefetch);

// Back/forward between list and post. Capture phase + stopImmediatePropagation keeps
// htmx's history handler from re-rendering the blog page we're already on.
window.addEventListener("popstate",e=>{
  if(!$("post-term")||location.pathname!=="/blog")return;
  e.stopImmediatePropagation();
  // Our own history.back() from closing lands here later; a post opened meanwhile must stay open.
  if(ownBack){ownBack=false;return}
  const slug=new URLSearchParams(location.search).get("post");
  slug?openPost(slug,false):closePost();
},true);

// Direct load of /blog?post=…: the server already rendered the post into the terminal.
document.addEventListener("DOMContentLoaded",()=>{
  const term=$("post-term"),content=$("blog-post-content");
  const slug=new URLSearchParams(location.search).get("post");
  if(!term||!slug)return;
  if(content.innerHTML.trim()){
    postCache.set(slug,Promise.resolve(content.innerHTML));
    $("term-file").textContent=`${slug}.md`;
    term.showModal();
    $("term-body").focus({preventScroll:true});
  }else openPost(slug,false);
});

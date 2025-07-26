# Simple HTMX Splash Screen

## ✨ **Ultra-Simple Implementation**

The splash screen is now implemented using pure HTMX with minimal JavaScript - the cleanest possible approach!

## 🎯 How It Works

### **Pure HTMX Approach**
1. **Inline Script Check**: Checks `sessionStorage` before page renders
2. **Conditional Loading**: Only writes HTMX container if splash not shown
3. **HTMX Trigger**: Automatically loads splash content on page load
4. **Auto-Hide**: HTMX automatically swaps to empty div after 2.5s
5. **Session Persistence**: Marked as shown to prevent repeats

### **File Structure**
```
public/
├── index.html (inline script + HTMX container)
├── partials/ui/
│   ├── splash.html (splash content with HTMX auto-hide)
│   └── splash-hide.html (empty replacement div)
└── server: /splash endpoint
```

## 🔧 Implementation Details

### **Client-Side (index.html)**
```html
<script>
  if (!sessionStorage.getItem('splashShown')) {
    document.write(`<div hx-get="/splash" hx-trigger="load"></div>`);
    sessionStorage.setItem('splashShown', 'true');
  }
</script>
```

### **Splash Content (splash.html)**
```html
<div id="splash-screen" 
     hx-trigger="load delay:2.5s"
     hx-swap="outerHTML"
     hx-get="/partials/ui/splash-hide.html">
  <!-- Splash content with inline styles -->
</div>
```

### **Server Endpoint**
- `GET /splash` → serves `splash.html`
- `GET /partials/ui/splash-hide.html` → serves empty div

## ✅ **Benefits of This Approach**

- **🚀 Zero JavaScript complexity** - just HTMX
- **📦 Minimal code** - under 20 lines total
- **⚡ Fast performance** - no DOM manipulation
- **🔧 Easy maintenance** - pure declarative approach
- **🎨 Clean separation** - HTML for structure, HTMX for behavior

## 🛠️ **Testing**

### Reset and Test Again
```javascript
resetSplash() // In browser console
```

### Manual Testing
1. Open in incognito window → splash shows
2. Refresh same tab → splash doesn't show
3. New incognito window → splash shows again

## 📱 **Features**

- ✅ **Session-based**: Shows once per browser session
- ✅ **Responsive design**: Works on all screen sizes  
- ✅ **Auto-timing**: 2.5 second display with smooth transition
- ✅ **Clean removal**: No DOM pollution after hide
- ✅ **Server-side**: Proper separation of concerns

---

## 🏆 **Why This Is Better**

### **Previous Approaches Had**:
- Complex JavaScript classes
- Manual DOM manipulation
- Timing coordination issues
- Large CSS files

### **Current Approach Has**:
- Pure HTMX declarative syntax
- Server-side content serving
- Automatic timing with `delay:` 
- Inline styles (no external CSS needed)

**Result**: The simplest possible splash screen that leverages HTMX's power! 🎉

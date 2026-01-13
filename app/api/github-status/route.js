import { NextResponse } from 'next/server';

export const runtime = 'edge';

const timeUnits = [
  [60, 's'], [3600, 'm'], [86400, 'h'], [604800, 'd'],
  [2592000, 'w'], [31536000, 'mo'], [Infinity, 'y']
];

function relativeTime(iso) {
  const diff = Math.floor((Date.now() - new Date(iso)) / 1000);
  if (diff < 60) return 'just now';
  for (let i = 1; i < timeUnits.length; i++) {
    if (diff < timeUnits[i][0]) return `${Math.floor(diff / timeUnits[i-1][0])}${timeUnits[i][1]} ago`;
  }
  return `${Math.floor(diff / 31536000)}y ago`;
}

export async function GET() {
  const repo = process.env.GITHUB_REPO || '0xjah/0xjah.xyz';
  let time = 'recently';

  try {
    const res = await fetch(`https://api.github.com/repos/${repo}`, {
      headers: { 'User-Agent': '0xjah.xyz/1.0' },
    });
    if (res.ok) {
      const { pushed_at } = await res.json();
      if (pushed_at) time = relativeTime(pushed_at);
    }
  } catch (e) {
    console.error('GitHub status error:', e);
  }

  return new NextResponse(
    `<div class="status-header"><h2>Status</h2><p class="quote">Last updated: ${time}</p></div>`,
    { headers: { 'Content-Type': 'text/html' } }
  );
}

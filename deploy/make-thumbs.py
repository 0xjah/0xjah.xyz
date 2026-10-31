#!/usr/bin/env python3
"""Make small WebP thumbnails for the gallery and upload them to R2 under thumbs/.

The server uses thumbs/<key>.webp in the grid when it exists, and the original otherwise.
Run this after adding images to the bucket. Images that already have a thumbnail are skipped.

    pip install boto3 pillow
    python3 deploy/make-thumbs.py            # reads R2_* from .env or the environment
    python3 deploy/make-thumbs.py --dry-run  # only list what would be made
"""
import io
import os
import sys

import boto3
from PIL import Image, ImageOps

WIDTH = 640     # tiles are ~270px wide, so 640 stays sharp on 2x screens
QUALITY = 78
PREFIX = "thumbs/"
IMAGE_EXTS = (".jpg", ".jpeg", ".png", ".webp")  # same list the server accepts


def load_env(path=".env"):
    if not os.path.exists(path):
        return
    for line in open(path):
        line = line.strip()
        if line and not line.startswith("#") and "=" in line:
            k, v = line.split("=", 1)
            os.environ.setdefault(k, v)


def make_thumb(data):
    im = ImageOps.exif_transpose(Image.open(io.BytesIO(data)))
    if im.mode not in ("RGB", "RGBA"):
        im = im.convert("RGBA" if "transparency" in im.info else "RGB")
    if im.width > WIDTH:
        im = im.resize((WIDTH, round(im.height * WIDTH / im.width)), Image.LANCZOS)
    out = io.BytesIO()
    im.save(out, "WEBP", quality=QUALITY, method=6)
    return out.getvalue()


def main():
    dry = "--dry-run" in sys.argv
    load_env()
    bucket = os.environ["R2_BUCKET"]
    s3 = boto3.client(
        "s3",
        endpoint_url="https://" + os.environ["R2_ENDPOINT"],
        aws_access_key_id=os.environ["R2_ACCESS_KEY"],
        aws_secret_access_key=os.environ["R2_SECRET_KEY"],
        region_name="auto",
    )

    keys = []
    for page in s3.get_paginator("list_objects_v2").paginate(Bucket=bucket):
        keys += [o["Key"] for o in page.get("Contents", [])]
    have = {k for k in keys if k.startswith(PREFIX)}
    todo = [k for k in keys
            if not k.startswith(PREFIX) and k.lower().endswith(IMAGE_EXTS)
            and f"{PREFIX}{k}.webp" not in have]

    print(f"{len(todo)} thumbnail(s) to make, {len(have)} already exist")
    before = after = 0
    for key in todo:
        data = s3.get_object(Bucket=bucket, Key=key)["Body"].read()
        thumb = make_thumb(data)
        before += len(data)
        after += len(thumb)
        print(f"  {key}: {len(data) // 1024} KB -> {len(thumb) // 1024} KB")
        if not dry:
            s3.put_object(Bucket=bucket, Key=f"{PREFIX}{key}.webp", Body=thumb,
                          ContentType="image/webp",
                          CacheControl="public, max-age=31536000, immutable")
    if todo:
        print(f"total {before // 1024} KB -> {after // 1024} KB" + (" (dry run, nothing uploaded)" if dry else ""))


if __name__ == "__main__":
    main()

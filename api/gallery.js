import { S3Client, ListObjectsV2Command } from '@aws-sdk/client-s3';

export const config = {
  runtime: 'edge',
};

export default async function handler(request) {
  const r2Endpoint = process.env.R2_ENDPOINT;
  const r2AccessKey = process.env.R2_ACCESS_KEY;
  const r2SecretKey = process.env.R2_SECRET_KEY;
  const r2Bucket = process.env.R2_BUCKET;
  const r2PublicUrl = process.env.R2_PUBLIC_URL;

  if (!r2AccessKey || !r2SecretKey || !r2Endpoint || !r2Bucket) {
    return new Response(JSON.stringify({ images: [], count: 0 }), {
      status: 200,
      headers: {
        'Content-Type': 'application/json',
        'Access-Control-Allow-Origin': '*',
      },
    });
  }

  try {
    const client = new S3Client({
      region: 'auto',
      endpoint: `https://${r2Endpoint}`,
      credentials: {
        accessKeyId: r2AccessKey,
        secretAccessKey: r2SecretKey,
      },
    });

    const command = new ListObjectsV2Command({
      Bucket: r2Bucket,
    });

    const response = await client.send(command);
    const contents = response.Contents || [];

    const images = contents
      .filter((item) => {
        const key = item.Key?.toLowerCase() || '';
        return key.endsWith('.jpg') || key.endsWith('.jpeg') || key.endsWith('.png') || key.endsWith('.webp');
      })
      .map((item) => {
        const key = item.Key;
        const baseUrl = r2PublicUrl || `${r2Bucket}.${r2Endpoint}`;
        return {
          name: key,
          key: key,
          url: `https://${baseUrl}/${key}`,
        };
      })
      .sort((a, b) => a.key.localeCompare(b.key));

    return new Response(JSON.stringify({ images, count: images.length }), {
      status: 200,
      headers: {
        'Content-Type': 'application/json',
        'Access-Control-Allow-Origin': '*',
      },
    });
  } catch (error) {
    console.error('Gallery error:', error);
    return new Response(JSON.stringify({ error: 'Failed to fetch from R2', images: [], count: 0 }), {
      status: 500,
      headers: {
        'Content-Type': 'application/json',
        'Access-Control-Allow-Origin': '*',
      },
    });
  }
}

#!/usr/bin/env node
/**
 * @file barcode-resolver.js
 * @brief MQTT-based barcode lookup service using BarcodeLookup API
 * 
 * Features:
 * - Connects to Mosquitto broker at desk.local:1883
 * - Resolves UPC codes via BarcodeLookup API
 * - Publishes product information back to ESP32 devices
 * - Error handling with timeout and retry logic
 */

const mqtt = require('mqtt');
const fastify = require('fastify')({ logger: false });
const sharp = require('sharp');
const crypto = require('crypto');
require('dotenv').config();

// Configuration
const MQTT_BROKER_URI = 'mqtt://desk.local:1883';
const BARCODE_API_BASE = 'https://api.barcodelookup.com/v3/products';
const REQUEST_TIMEOUT_MS = 10000;  // 10 second timeout
const MAX_RETRIES = 3;
const HTTP_PROXY_PORT = 3000;

// Validate API key
if (!process.env.BARCODELOOKUP_API_KEY) {
    console.error('ERROR: BARCODELOOKUP_API_KEY not found in .env file');
    process.exit(1);
}

const TAG = 'barcode-resolver';

// Image cache for proxy server
const imageCache = new Map();

// Clear cache on startup to prevent serving stale images
console.log(`[${TAG}] Clearing image cache on startup`);
imageCache.clear();

console.log(`[${TAG}] Starting barcode resolver service...`);
console.log(`[${TAG}] Connecting to MQTT broker: ${MQTT_BROKER_URI}`);

// Connect to MQTT broker
const client = mqtt.connect(MQTT_BROKER_URI, {
    clientId: `barcode-resolver-${Math.random().toString(16).substr(2, 8)}`,
    keepalive: 30,
    reconnectPeriod: 5000,
    connectTimeout: 10000,
});

// Setup Fastify HTTP proxy server with image processing

// Image proxy endpoint with Sharp resizing
fastify.get('/image/:imageId', async (request, reply) => {
    const imageId = request.params.imageId;
    const imageUrl = request.query.url;
    const width = parseInt(request.query.w) || 80;  // Default to 80x80 for ESP32
    const height = parseInt(request.query.h) || 80;
    const nocache = request.query.nocache === '1';
    
    console.log(`[${TAG}] Proxying image: ${imageId} (${width}x${height}) from ${imageUrl}${nocache ? ' [NOCACHE]' : ''}`);
    
    // Create unique cache key using URL hash to prevent collisions
    const urlHash = crypto.createHash('md5').update(imageUrl || '').digest('hex').substring(0, 8);
    const cacheKey = `${urlHash}_${width}x${height}`;
    
    // Check cache first (unless nocache is requested)
    if (!nocache && imageCache.has(cacheKey)) {
        const cachedImage = imageCache.get(cacheKey);
        console.log(`[${TAG}] Cache HIT: Serving cached image ${cacheKey} for URL: ${imageUrl}`);
        return reply
            .type(cachedImage.contentType)
            .send(cachedImage.buffer);
    } else if (!nocache) {
        console.log(`[${TAG}] Cache MISS: Processing new image ${cacheKey} for URL: ${imageUrl}`);
    }
    
    if (!imageUrl) {
        console.error(`[${TAG}] No URL provided for image ${imageId}`);
        return reply.code(400).send('Missing image URL');
    }
    
    try {
        // Download original image
        console.log(`[${TAG}] Downloading image from: ${imageUrl}`);
        const controller = new AbortController();
        const timeoutId = setTimeout(() => controller.abort(), REQUEST_TIMEOUT_MS);
        
        const response = await fetch(imageUrl, {
            method: 'GET',
            signal: controller.signal,
            headers: {
                'User-Agent': 'ESP32-Barcode-Scanner-Proxy/1.0'
            }
        });
        
        clearTimeout(timeoutId);
        
        if (!response.ok) {
            console.error(`[${TAG}] Image download failed: ${response.status} ${response.statusText}`);
            return reply.code(404).send('Image not found');
        }
        
        const originalBuffer = Buffer.from(await response.arrayBuffer());
        console.log(`[${TAG}] Original image: ${originalBuffer.length} bytes`);
        
        // Convert to raw RGB565 format for LVGL
        const rawBuffer = await sharp(originalBuffer)
            .resize(width, height, {
                fit: 'cover',        // Cover the entire area
                position: 'center'   // Center the image
            })
            .raw()                   // Get raw RGB888 data
            .toBuffer();
        
        console.log(`[${TAG}] Raw RGB888 data: ${rawBuffer.length} bytes`);
        
        // Convert RGB888 to RGB565 with correct byte order for ESP32/LVGL
        const rgb565Buffer = Buffer.alloc(width * height * 2);
        for (let i = 0, j = 0; i < rawBuffer.length; i += 3, j += 2) {
            const r = rawBuffer[i] >> 3;     // 5 bits for red (0-31)
            const g = rawBuffer[i + 1] >> 2; // 6 bits for green (0-63)  
            const b = rawBuffer[i + 2] >> 3; // 5 bits for blue (0-31)
            
            // RGB565 format: RRRRRGGG GGGBBBBB
            const rgb565 = (r << 11) | (g << 5) | b;
            
            // Try big-endian byte order (high byte first)
            rgb565Buffer[j] = (rgb565 >> 8) & 0xFF;     // High byte first
            rgb565Buffer[j + 1] = rgb565 & 0xFF;        // Low byte second
        }
        
        // Log first few pixels for debugging
        if (rgb565Buffer.length >= 8) {
            console.log(`[${TAG}] First 4 RGB565 pixels: ${rgb565Buffer.subarray(0, 8).toString('hex')}`);
        }
        
        console.log(`[${TAG}] RGB565 data: ${rgb565Buffer.length} bytes (${Math.round(rgb565Buffer.length * 100 / originalBuffer.length)}% of original)`);
        
        // Cache the RGB565 data (expires in 1 hour) unless nocache requested
        if (!nocache) {
            imageCache.set(cacheKey, {
                buffer: rgb565Buffer,
                contentType: 'application/octet-stream',
                width: width,
                height: height,
                timestamp: Date.now(),
                originalUrl: imageUrl
            });
            console.log(`[${TAG}] Cached image ${cacheKey} for URL: ${imageUrl}`);
        } else {
            console.log(`[${TAG}] Skipping cache for ${cacheKey} (nocache requested)`);
        }
        
        return reply
            .type('application/octet-stream')
            .header('X-Image-Format', 'RGB565')
            .header('X-Image-Width', width.toString())
            .header('X-Image-Height', height.toString())
            .send(rgb565Buffer);
        
    } catch (error) {
        if (error.name === 'AbortError') {
            console.error(`[${TAG}] Image download timeout: ${imageUrl}`);
        } else {
            console.error(`[${TAG}] Image processing error: ${error.message}`);
        }
        return reply.code(500).send('Image processing failed');
    }
});

// Start Fastify server
const startServer = async () => {
    try {
        await fastify.listen({ port: HTTP_PROXY_PORT, host: '0.0.0.0' });
        console.log(`[${TAG}] Fastify image proxy server listening on port ${HTTP_PROXY_PORT}`);
    } catch (err) {
        console.error(`[${TAG}] Failed to start Fastify server:`, err);
        process.exit(1);
    }
};
startServer();

// Clean cache every 30 minutes
setInterval(() => {
    const now = Date.now();
    let cleanedCount = 0;
    for (const [key, value] of imageCache.entries()) {
        if (now - value.timestamp > 3600000) { // 1 hour
            imageCache.delete(key);
            cleanedCount++;
        }
    }
    if (cleanedCount > 0) {
        console.log(`[${TAG}] Cleaned ${cleanedCount} expired images from cache`);
    }
}, 1800000); // 30 minutes

/**
 * Lookup barcode using BarcodeLookup API
 * @param {string} barcode - UPC/EAN barcode to lookup
 * @returns {Promise<Object>} Product information or null if not found
 */
async function lookupBarcode(barcode) {
    const url = `${BARCODE_API_BASE}?barcode=${barcode}&formatted=y&key=${process.env.BARCODELOOKUP_API_KEY}`;
    
    console.log(`[${TAG}] Looking up barcode: ${barcode}`);
    
    try {
        const controller = new AbortController();
        const timeoutId = setTimeout(() => controller.abort(), REQUEST_TIMEOUT_MS);
        
        const response = await fetch(url, {
            method: 'GET',
            signal: controller.signal,
            headers: {
                'User-Agent': 'ESP32-Barcode-Scanner/1.0'
            }
        });
        
        clearTimeout(timeoutId);
        
        if (!response.ok) {
            console.error(`[${TAG}] API request failed: ${response.status} ${response.statusText}`);
            return null;
        }
        
        const data = await response.json();
        
        // Log the raw API response for debugging
        console.log(`[${TAG}] Raw API response:`, JSON.stringify(data, null, 2));
        
        if (data.products && data.products.length > 0) {
            const product = data.products[0];
            
            // Log detailed product information
            console.log(`[${TAG}] Product details:`, JSON.stringify(product, null, 2));
            console.log(`[${TAG}] Images array:`, product.images);
            console.log(`[${TAG}] Selected image URL:`, product.images?.[0]);
            
            console.log(`[${TAG}] Found product: "${product.title || product.product_name || 'Unknown'}" by ${product.brand || 'Unknown Brand'} (Model: ${product.mpn || product.model || 'N/A'})`);
            
            // Return only essential data to prevent ESP32 memory issues
            return {
                name: product.title || product.product_name || 'Unknown Product',
                brand: product.brand || 'Unknown Brand', 
                model: product.mpn || product.model || '',
                price: product.stores && product.stores.length > 0 
                    ? `$${product.stores[0].price || 'N/A'}` 
                    : 'Price N/A',
                image_url: product.images && Array.isArray(product.images) && product.images.length > 0 
                    ? (typeof product.images[0] === 'string' ? 
                        `http://desk.local:${HTTP_PROXY_PORT}/image/${crypto.createHash('md5').update(product.images[0]).digest('hex').substring(0, 16)}?url=${encodeURIComponent(product.images[0])}&w=80&h=80` 
                        : null) : null,
                upc: product.barcode_number || barcode
            };
        } else {
            console.log(`[${TAG}] No products found for barcode: ${barcode}`);
            return null;
        }
        
    } catch (error) {
        if (error.name === 'AbortError') {
            console.error(`[${TAG}] API request timeout for barcode: ${barcode}`);
        } else {
            console.error(`[${TAG}] API error for barcode ${barcode}:`, error.message);
        }
        return null;
    }
}

/**
 * Handle barcode lookup request from ESP32 device
 * @param {string} topic - MQTT topic
 * @param {Buffer} message - Message buffer
 */
async function handleBarcodeRequest(topic, message) {
    try {
        // Extract device ID from topic: barcode/lookup/request/{device_id}
        const deviceId = topic.split('/').pop();
        const responseTopic = `barcode/lookup/response/${deviceId}`;
        
        // Parse JSON request
        const request = JSON.parse(message.toString());
        const { barcode, request_id, timestamp } = request;
        
        if (!barcode) {
            console.error(`[${TAG}] Missing barcode in request from ${deviceId}`);
            return;
        }
        
        console.log(`[${TAG}] Processing request ${request_id} from ${deviceId}: ${barcode}`);
        
        const startTime = Date.now();
        
        // Lookup product information
        const product = await lookupBarcode(barcode);
        
        const lookupTime = Date.now() - startTime;
        
        // Prepare response
        const response = {
            request_id: request_id || 'unknown',
            success: !!product,
            barcode: barcode,
            product: product,
            lookup_time_ms: lookupTime,
            timestamp: Math.floor(Date.now() / 1000)
        };
        
        // Publish response
        client.publish(responseTopic, JSON.stringify(response), { qos: 1 }, (error) => {
            if (error) {
                console.error(`[${TAG}] Failed to publish response:`, error);
            } else {
                console.log(`[${TAG}] Published response to ${deviceId}: ${product ? 'SUCCESS' : 'NOT_FOUND'} (${lookupTime}ms)`);
            }
        });
        
    } catch (error) {
        console.error(`[${TAG}] Error processing barcode request:`, error);
    }
}

// MQTT event handlers
client.on('connect', () => {
    console.log(`[${TAG}] Connected to MQTT broker`);
    
    // Subscribe to barcode lookup requests from all devices
    client.subscribe('barcode/lookup/request/+', { qos: 1 }, (error) => {
        if (error) {
            console.error(`[${TAG}] Failed to subscribe:`, error);
        } else {
            console.log(`[${TAG}] Subscribed to barcode/lookup/request/+`);
            console.log(`[${TAG}] Barcode resolver service ready!`);
        }
    });
});

client.on('message', (topic, message) => {
    if (topic.startsWith('barcode/lookup/request/')) {
        handleBarcodeRequest(topic, message);
    }
});

client.on('error', (error) => {
    console.error(`[${TAG}] MQTT Error:`, error);
});

client.on('offline', () => {
    console.log(`[${TAG}] MQTT client offline`);
});

client.on('reconnect', () => {
    console.log(`[${TAG}] MQTT reconnecting...`);
});

// Graceful shutdown
process.on('SIGINT', () => {
    console.log(`\n[${TAG}] Shutting down barcode resolver...`);
    client.end(() => {
        console.log(`[${TAG}] Disconnected from MQTT broker`);
        process.exit(0);
    });
});

process.on('SIGTERM', () => {
    console.log(`[${TAG}] Received SIGTERM, shutting down...`);
    client.end(() => {
        process.exit(0);
    });
});
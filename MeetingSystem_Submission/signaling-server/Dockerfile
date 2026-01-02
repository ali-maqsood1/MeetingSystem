# Use Node.js LTS
FROM node:18-alpine

# Set working directory
WORKDIR /app

# Copy package files
COPY package*.json ./

# Install dependencies
RUN npm install --production

# Copy server code
COPY server.js ./

# Expose port (Render will override with PORT env var)
EXPOSE 8181

# Start server
CMD ["node", "server.js"]

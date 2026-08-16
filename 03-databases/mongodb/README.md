# MongoDB

## Document Model

MongoDB stores BSON documents in collections. No fixed schema — each document can have different fields.

```js
// A document
{
  _id: ObjectId("..."),
  name: "Alice",
  email: "alice@example.com",
  address: {                    // embedded document
    city: "London",
    postcode: "SW1A 1AA"
  },
  tags: ["admin", "verified"], // array
  createdAt: ISODate("2024-01-01")
}
```

---

## Schema Design: Embed vs Reference

The most important MongoDB design decision.

### Embed when:
- The data is always accessed together with the parent
- The embedded data is small and bounded in size
- You need atomic updates across both (one document = one atomic write)

```js
// Good embed: address is always fetched with the user
{ _id: "user1", name: "Alice", address: { city: "London", postcode: "SW1" } }
```

### Reference when:
- The child document is large or unbounded (e.g. a post with unlimited comments)
- The child is referenced by multiple parents
- You need to query the child independently

```js
// Reference: order references user by ID
{ _id: "order1", userId: "user1", total: 99.99 }
```

### The 16MB Rule
A single document cannot exceed 16MB. Unbounded arrays (comments, events, log entries) will eventually blow this limit. Reference or use a separate collection with a parent ID.

---

## Indexes

```js
// Single field
db.users.createIndex({ email: 1 })  // 1 = ascending, -1 = descending

// Compound (order and direction matter for sort)
db.orders.createIndex({ userId: 1, createdAt: -1 })

// Text index for full-text search
db.articles.createIndex({ title: "text", body: "text" })

// TTL index — automatically delete documents after expiry
db.sessions.createIndex({ createdAt: 1 }, { expireAfterSeconds: 3600 })

// Sparse index — only indexes documents where the field exists
db.users.createIndex({ phone: 1 }, { sparse: true })

// Unique
db.users.createIndex({ email: 1 }, { unique: true })
```

---

## Aggregation Pipeline

Transforms documents through sequential stages. Each stage passes results to the next.

```js
db.orders.aggregate([
  // Stage 1: filter
  { $match: { status: "completed", createdAt: { $gte: new Date("2024-01-01") } } },

  // Stage 2: join with users collection
  { $lookup: {
    from: "users",
    localField: "userId",
    foreignField: "_id",
    as: "user"
  }},

  // Stage 3: unwind the joined array (lookup returns array)
  { $unwind: "$user" },

  // Stage 4: group by country, sum revenue
  { $group: {
    _id: "$user.country",
    totalRevenue: { $sum: "$amount" },
    orderCount: { $count: {} }
  }},

  // Stage 5: sort by revenue descending
  { $sort: { totalRevenue: -1 } },

  // Stage 6: limit results
  { $limit: 10 },

  // Stage 7: reshape output
  { $project: { country: "$_id", totalRevenue: 1, orderCount: 1, _id: 0 } }
])
```

---

## Mongoose (TypeScript)

```typescript
import mongoose, { Schema, Document, Model } from 'mongoose'

interface IUser extends Document {
  name: string
  email: string
  role: 'user' | 'admin'
  createdAt: Date
}

const userSchema = new Schema<IUser>({
  name: { type: String, required: true, trim: true },
  email: {
    type: String,
    required: true,
    unique: true,
    lowercase: true,
    match: [/^\S+@\S+\.\S+$/, 'Invalid email']
  },
  role: { type: String, enum: ['user', 'admin'], default: 'user' },
}, { timestamps: true })

// Index
userSchema.index({ email: 1 })

// Virtual (not stored in DB)
userSchema.virtual('displayName').get(function() {
  return this.name.toUpperCase()
})

// Pre-save hook
userSchema.pre('save', async function(next) {
  if (this.isModified('password')) {
    this.password = await bcrypt.hash(this.password, 12)
  }
  next()
})

const User: Model<IUser> = mongoose.model<IUser>('User', userSchema)
```

### Populate (Reference joins)
```typescript
// Order schema references User
const orderSchema = new Schema({
  userId: { type: Schema.Types.ObjectId, ref: 'User', required: true },
  total: Number
})

// Populate at query time
const orders = await Order
  .find({ status: 'completed' })
  .populate('userId', 'name email')  // only fetch name and email fields
  .lean()  // returns plain JS objects, not Mongoose documents (faster)
```

---

## Transactions

Multi-document ACID transactions (requires replica set, available since 4.0):

```typescript
const session = await mongoose.startSession()
session.startTransaction()
try {
  await Account.updateOne({ _id: fromId }, { $inc: { balance: -amount } }, { session })
  await Account.updateOne({ _id: toId }, { $inc: { balance: amount } }, { session })
  await session.commitTransaction()
} catch (err) {
  await session.abortTransaction()
  throw err
} finally {
  session.endSession()
}
```

---

## Common Performance Mistakes

1. **No index on query fields** — MongoDB does a collection scan
2. **Unbounded arrays** — grows past 16MB document limit
3. **Not using projection** — fetching entire documents when you need 2 fields
4. **$where queries** — runs JavaScript, can't use indexes
5. **Not using lean()** — Mongoose hydrates documents; `.lean()` returns raw objects (2-10x faster for reads)
6. **Missing compound index direction** — `{ a: 1, b: -1 }` is different from `{ a: 1, b: 1 }` for sort

---

## Interview Questions

**Q: When would you choose MongoDB over PostgreSQL?**
MongoDB fits when: schema is genuinely variable (different attributes per document), you're working with nested/hierarchical data that maps naturally to documents, development speed is more important than strict consistency, you're doing event logging or time-series data. PostgreSQL fits for: relational data, financial data requiring ACID, complex reporting with joins, anything where schema integrity matters.

**Q: How do you handle many-to-many relationships in MongoDB?**
Two common approaches: (1) Store an array of references in both documents — query each side separately. (2) Create a join collection just like SQL. The right choice depends on query patterns — if you always traverse one direction, embed the IDs in that document only.

**Q: What is the aggregation pipeline and how is it different from map-reduce?**
The aggregation pipeline is a series of transformation stages applied in order. It runs natively in C++ inside the database engine and is far faster than map-reduce (JavaScript). Pipeline stages can use indexes, pipeline operations are composable, and the execution plan is optimizable. Map-reduce is largely deprecated in modern MongoDB.
